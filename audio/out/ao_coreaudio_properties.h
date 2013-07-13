/*
 * This file is part of mpv.
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
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

#ifndef MPV_COREAUDIO_PROPERTIES_H
#define MPV_COREAUDIO_PROPERTIES_H

#include <AudioToolbox/AudioToolbox.h>

// CoreaAudio names are way too verbose

#define ca_sel    AudioObjectPropertySelector
#define ca_scope  AudioObjectPropertyScope
#define CA_GLOBAL kAudioObjectPropertyScopeGlobal
#define CA_OUTPUT kAudioObjectPropertyScopeOutut

OSStatus ca_get(AudioObjectID id, ca_scope scope, ca_sel selector,
                uint32_t size, void *data);

OSStatus ca_set(AudioObjectID id, ca_scope scope, ca_sel selector,
                uint32_t size, void *data);

#define CA_GET(id, sel, data) ca_get(id, CA_GLOBAL, sel, sizeof(*(data)), data)
#define CA_SET(id, sel, data) ca_set(id, CA_GLOBAL, sel, sizeof(*(data)), data)

uint32_t GetAudioPropertyArray(AudioObjectID id,
                               AudioObjectPropertySelector selector,
                               AudioObjectPropertyScope scope, void **data);

uint32_t GetGlobalAudioPropertyArray(AudioObjectID id,
                                     AudioObjectPropertySelector selector,
                                     void **data);

OSStatus GetAudioPropertyString(AudioObjectID id,
                                AudioObjectPropertySelector selector,
                                char **data);

Boolean IsAudioPropertySettable(AudioObjectID id,
                                AudioObjectPropertySelector selector,
                                Boolean *outData);

#endif /* MPV_COREAUDIO_PROPERTIES_H */
