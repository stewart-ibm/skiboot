/* Copyright 2013-2017 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __TRUSTEDBOOT_H
#define __TRUSTEDBOOT_H

#include <platform.h>

void trustedboot_init(void);

/**
 * trustedboot_measure - measure a resource
 * @id    : resource id
 * @buf   : data to be measured
 * @len   : buf length
 *
 * This measures a resource downloaded from PNOR if trusted mode is on. That is,
 * an EV_ACTION event is recorded in the event log for the mapped PCR, and the
 * the sha1 and sha256 measurements are extended in the mapped PCR.
 *
 * For more information please refer to 'doc/stb.rst'
 *
 * returns: 0 or an error as defined in status_codes.h
 */
int trustedboot_measure(enum resource_id id, void *buf, size_t len);

#endif /* __TRUSTEDBOOT_H */
