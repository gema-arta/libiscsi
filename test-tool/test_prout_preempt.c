/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2015 David Disseldorp

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <arpa/inet.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"
#include "iscsi-multipath.h"

void
test_prout_preempt_rm_reg(void)
{
        int ret = 0;
        const unsigned long long k1 = rand_key();
        const unsigned long long k2 = rand_key();
        struct scsi_device *sd2;
        struct scsi_task *tsk;
        uint32_t old_gen;
        int num_uas;
        struct scsi_persistent_reserve_in_read_keys *rk;

        CHECK_FOR_DATALOSS;

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This PERSISTENT RESERVE test is "
                        "only supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test Persistent Reserve IN PREEMPT works.");

        ret = prout_register_and_ignore(sd, k1);
        if (ret == -2) {
                CU_PASS("PERSISTENT RESERVE OUT is not implemented.");
                return;
        }
        CU_ASSERT_EQUAL(ret, 0);

        /* clear all PR state */
        ret = prout_clear(sd, k1);
        CU_ASSERT_EQUAL(ret, 0);

        /* need to reregister cleared key */
        ret = prout_register_and_ignore(sd, k1);
        CU_ASSERT_EQUAL(ret, 0);

        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);

        if (ret < 0)
                return;

        /* register secondary key */
        ret = prout_register_and_ignore(sd2, k2);
        CU_ASSERT_EQUAL(ret, 0);

        /* confirm that k1 and k2 are registered */
        ret = prin_read_keys(sd, &tsk, &rk, 16384);
        CU_ASSERT_EQUAL_FATAL(ret, 0);

        CU_ASSERT_EQUAL(rk->num_keys, 2);
        /* retain PR generation number to check for increments */
        old_gen = rk->prgeneration;

        scsi_free_scsi_task(tsk);
        rk = NULL;        /* freed with tsk */

        /* use second connection to clear k1 registration */
        ret = prout_preempt(sd2, k1, k2,
                            SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS);
        CU_ASSERT_EQUAL(ret, 0);

        /* clear any UAs generated by preempt */
        ret = test_iscsi_tur_until_good(sd, &num_uas);
        CU_ASSERT_EQUAL(ret, 0);
        ret = test_iscsi_tur_until_good(sd2, &num_uas);
        CU_ASSERT_EQUAL(ret, 0);

        ret = prin_read_keys(sd, &tsk, &rk, 16384);
        CU_ASSERT_EQUAL_FATAL(ret, 0);

        CU_ASSERT_EQUAL(rk->num_keys, 1);
        /* ensure preempt bumped generation number */
        CU_ASSERT_EQUAL(rk->prgeneration, old_gen + 1);
        /* ensure k2 is retained */
        CU_ASSERT_EQUAL(rk->keys[0], k2);

        /* unregister k2 */
        ret = prout_register_key(sd2, 0, k2);
        CU_ASSERT_EQUAL(ret, 0);
}
