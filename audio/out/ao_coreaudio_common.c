/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This file contains functions interacting with the CoreAudio framework
 * that are not specific to the AUHAL. These are split in a separate file for
 * the sake of readability. In the future the could be used by other AOs based
 * on CoreAudio but not the AUHAL (such as using AudioQueue services).
 */

#include <AudioUnit/AudioUnit.h>
#include <inttypes.h>
#include <stdbool.h>
#include "core/mp_msg.h"

#define ca_msg(a, b ...) mp_msg(MSGT_AO, a, "AO: [coreaudio] " b)
#define CA_CFSTR_ENCODING kCFStringEncodingASCII

static char *fourcc_repr(void *talloc_ctx, uint32_t code)
{
    // Extract FourCC letters from the uint32_t and finde out if it's a valid
    // code that is made of letters.
    char fcc[4] = {
        (code >> 24) & 0xFF,
        (code >> 16) & 0xFF,
        (code >> 8)  & 0xFF,
        code         & 0xFF,
    };

    bool valid_fourcc = true;
    for (int i = 0; i < 4; i++)
        if (!isprint(fcc[i]))
            valid_fourcc = false;

    char *repr;
    if (valid_fourcc)
        repr = talloc_asprintf(talloc_ctx, "'%c%c%c%c'",
                               fcc[0], fcc[1], fcc[2], fcc[3]);
    else
        repr = talloc_asprintf(NULL, "%d", code);

    return repr;
}

static bool check_ca_st(int level, OSStatus code, const char *message)
{
    if (code == noErr) return true;

    char *error_string = fourcc_repr(NULL, code);
    ca_msg(level, "%s (%s)\n", message, error_string);
    talloc_free(error_string);

    return false;
}

#define CHECK_CA_ERROR_L(label, message) \
    do { \
        if (!check_ca_st(MSGL_ERR, err, message)) { \
            goto label; \
        } \
    } while (0)

#define CHECK_CA_ERROR(message) CHECK_CA_ERROR_L(coreaudio_error, message)

static void ca_print_asbd(const char *description,
                          const AudioStreamBasicDescription *asbd)
{
    uint32_t flags  = asbd->mFormatFlags;
    char *format    = fourcc_repr(NULL, asbd->mFormatID);

    ca_msg(MSGL_V,
       "%s %7.1fHz %" PRIu32 "bit [%s]"
       "[%" PRIu32 "][%" PRIu32 "][%" PRIu32 "]"
       "[%" PRIu32 "][%" PRIu32 "] "
       "%s %s %s%s%s%s\n",
       description, asbd->mSampleRate, asbd->mBitsPerChannel, format,
       asbd->mFormatFlags, asbd->mBytesPerPacket, asbd->mFramesPerPacket,
       asbd->mBytesPerFrame, asbd->mChannelsPerFrame,
       (flags & kAudioFormatFlagIsFloat) ? "float" : "int",
       (flags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE",
       (flags & kAudioFormatFlagIsSignedInteger) ? "S" : "U",
       (flags & kAudioFormatFlagIsPacked) ? " packed" : "",
       (flags & kAudioFormatFlagIsAlignedHigh) ? " aligned" : "",
       (flags & kAudioFormatFlagIsNonInterleaved) ? " P" : "");

    talloc_free(format);
}

static OSStatus GetAudioProperty(AudioObjectID id,
                                 AudioObjectPropertySelector selector,
                                 UInt32 outSize, void *outData)
{
    AudioObjectPropertyAddress p_addr;

    p_addr.mSelector = selector;
    p_addr.mScope    = kAudioObjectPropertyScopeGlobal;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    return AudioObjectGetPropertyData(id, &p_addr, 0, NULL, &outSize,
                                      outData);
}

static UInt32 GetAudioPropertyArray(AudioObjectID id,
                                    AudioObjectPropertySelector selector,
                                    AudioObjectPropertyScope scope,
                                    void **data)
{
    OSStatus err;
    AudioObjectPropertyAddress p_addr;
    UInt32 p_size;

    p_addr.mSelector = selector;
    p_addr.mScope    = scope;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    err = AudioObjectGetPropertyDataSize(id, &p_addr, 0, NULL, &p_size);
    CHECK_CA_ERROR("Can't fetch property size");

    *data = malloc(p_size);

    err = AudioObjectGetPropertyData(id, &p_addr, 0, NULL, &p_size, *data);
    CHECK_CA_ERROR_L(coreaudio_error_free, "Can't fetch property data %s");

    return p_size;

coreaudio_error_free:
    free(*data);
coreaudio_error:
    return 0;
}

static UInt32 GetGlobalAudioPropertyArray(AudioObjectID id,
                                          AudioObjectPropertySelector selector,
                                          void **outData)
{
    return GetAudioPropertyArray(id, selector, kAudioObjectPropertyScopeGlobal,
                                 outData);
}

static OSStatus GetAudioPropertyString(AudioObjectID id,
                                       AudioObjectPropertySelector selector,
                                       char **data)
{
    OSStatus err;
    AudioObjectPropertyAddress p_addr;
    UInt32 p_size = sizeof(CFStringRef);
    CFStringRef string;

    p_addr.mSelector = selector;
    p_addr.mScope    = kAudioObjectPropertyScopeGlobal;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    err = AudioObjectGetPropertyData(id, &p_addr, 0, NULL, &p_size, &string);
    CHECK_CA_ERROR("Can't fetch array property");

    CFIndex size =
        CFStringGetMaximumSizeForEncoding(
            CFStringGetLength(string), CA_CFSTR_ENCODING) + 1;

    *data = malloc(size);
    CFStringGetCString(string, *data, size, CA_CFSTR_ENCODING);
    CFRelease(string);
coreaudio_error:
    return err;
}

static OSStatus SetAudioProperty(AudioObjectID id,
                                 AudioObjectPropertySelector selector,
                                 UInt32 inDataSize, void *inData)
{
    AudioObjectPropertyAddress p_addr;

    p_addr.mSelector = selector;
    p_addr.mScope    = kAudioObjectPropertyScopeGlobal;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    return AudioObjectSetPropertyData(id, &p_addr, 0, NULL,
                                      inDataSize, inData);
}

static Boolean IsAudioPropertySettable(AudioObjectID id,
                                       AudioObjectPropertySelector selector,
                                       Boolean *outData)
{
    AudioObjectPropertyAddress p_addr;

    p_addr.mSelector = selector;
    p_addr.mScope    = kAudioObjectPropertyScopeGlobal;
    p_addr.mElement  = kAudioObjectPropertyElementMaster;

    return AudioObjectIsPropertySettable(id, &p_addr, outData);
}
