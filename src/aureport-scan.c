/*
* aureport-scan.c - Extract interesting fields and check for match
* Copyright (c) 2005-06, 2008 Red Hat Inc., Durham, North Carolina.
* All Rights Reserved. 
*
* This software may be freely redistributed and/or modified under the
* terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2, or (at your option) any
* later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING. If not, write to the
* Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
* Authors:
*   Steve Grubb <sgrubb@redhat.com>
*/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <pwd.h>
#include "libaudit.h"
#include "aureport-options.h"
#include "ausearch-parse.h"
#include "ausearch-string.h"
#include "ausearch-lookup.h"
#include "aureport-scan.h"

static void do_summary_total(llist *l);
static int per_event_summary(llist *l);
static int per_event_detailed(llist *l);

summary_data sd;

/* This function inits the counters */
void reset_counters(void)
{
	sd.changes = 0UL;
	sd.crypto = 0UL;
	sd.acct_changes = 0UL;
	sd.good_logins = 0UL;
	sd.bad_logins = 0UL;
	sd.good_auth = 0UL;
	sd.bad_auth = 0UL;
	sd.events = 0UL;
	sd.avcs = 0UL;
	sd.mac = 0UL;
	sd.failed_syscalls = 0UL;
	sd.anomalies = 0UL;
	sd.responses = 0UL;
	slist_create(&sd.users);
	slist_create(&sd.terms);
	slist_create(&sd.files);
	slist_create(&sd.hosts);
	slist_create(&sd.exes);
	slist_create(&sd.avc_objs);
	slist_create(&sd.keys);
	ilist_create(&sd.pids);
	ilist_create(&sd.sys_list);
	ilist_create(&sd.anom_list);
	ilist_create(&sd.mac_list);
	ilist_create(&sd.resp_list);
	ilist_create(&sd.crypto_list);
}

/* This function inits the counters */
void destroy_counters(void)
{
	sd.changes = 0UL;
	sd.crypto = 0UL;
	sd.acct_changes = 0UL;
	sd.good_logins = 0UL;
	sd.bad_logins = 0UL;
	sd.good_auth = 0UL;
	sd.bad_auth = 0UL;
	sd.events = 0UL;
	sd.avcs = 0UL;
	sd.mac = 0UL;
	sd.failed_syscalls = 0UL;
	sd.anomalies = 0UL;
	sd.responses = 0UL;
	slist_clear(&sd.users);
	slist_clear(&sd.terms);
	slist_clear(&sd.files);
	slist_clear(&sd.hosts);
	slist_clear(&sd.exes);
	slist_clear(&sd.avc_objs);
	slist_clear(&sd.keys);
	ilist_clear(&sd.pids);
	ilist_clear(&sd.sys_list);
	ilist_clear(&sd.anom_list);
	ilist_create(&sd.mac_list);
	ilist_clear(&sd.resp_list);
	ilist_create(&sd.crypto_list);
}

/* This function will return 0 on no match and 1 on match */
int classify_success(const llist *l)
{
//printf("%d,succ=%d:%d\n", l->head->type, event_failed, l->s.success);
	// If match only failed... 
	if (event_failed == F_FAILED)
		return l->s.success == S_FAILED ? 1 : 0;
	// If match only success... 
	if (event_failed == F_SUCCESS)
		return l->s.success == S_SUCCESS ? 1 : 0;
	// Otherwise...we don't care so pretend it matched
	return 1;
}

/* This function will return 0 on no match and 1 on match */
int classify_conf(const llist *l)
{
	int rc = 1;

	switch (l->head->type)
	{
		case AUDIT_CONFIG_CHANGE:
		case AUDIT_USYS_CONFIG:
			break;
		case AUDIT_ADD_USER:
			if (event_conf_act == C_DEL)
				rc = 0;
			break;
		case AUDIT_DEL_USER:
			if (event_conf_act == C_ADD)
				rc = 0;
			break;
		case AUDIT_ADD_GROUP:
			if (event_conf_act == C_DEL)
				rc = 0;
			break;
		case AUDIT_DEL_GROUP:
			if (event_conf_act == C_ADD)
				rc = 0;
			break;
		case AUDIT_MAC_CIPSOV4_ADD:
			if (event_conf_act == C_DEL)
				rc = 0;
			break;
		case AUDIT_MAC_CIPSOV4_DEL:
			if (event_conf_act == C_ADD)
				rc = 0;
			break;
		case AUDIT_MAC_MAP_ADD:
			if (event_conf_act == C_DEL)
				rc = 0;
			break;
		case AUDIT_MAC_MAP_DEL:
			if (event_conf_act == C_ADD)
				rc = 0;
			break;
		case AUDIT_MAC_IPSEC_ADDSA:
			if (event_conf_act == C_DEL)
				rc = 0;
			break;
		case AUDIT_MAC_IPSEC_DELSA:
			if (event_conf_act == C_ADD)
				rc = 0;
			break;
		case AUDIT_MAC_IPSEC_ADDSPD:
			if (event_conf_act == C_DEL)
				rc = 0;
			break;
		case AUDIT_MAC_IPSEC_DELSPD:
			if (event_conf_act == C_ADD)
				rc = 0;
			break;
		case AUDIT_MAC_UNLBL_STCADD:
			if (event_conf_act == C_DEL)
				rc = 0;
			break;
		case AUDIT_MAC_UNLBL_STCDEL:
			if (event_conf_act == C_ADD)
				rc = 0;
			break;
		default:
			break;
	}
//printf("conf=%d:%d\n", l->head->type, rc);
	return rc;
}
 
/*
 * This function performs that matching of search params with the record.
 * It returns 1 on a match, and 0 if no match.
 */
int scan(llist *l)
{
	// Are we within time range?
	if (start_time == 0 || l->e.sec >= start_time) {
		if (end_time == 0 || l->e.sec <= end_time) {
			// OK - do the heavier checking
			int rc = extract_search_items(l);
			if (rc == 0) {
                                if (event_node) {
                                        if (l->e.node == NULL)
                                                return 0;
                                        if (strcasecmp(event_node, l->e.node))
                                                return 0;
                                }
				if (classify_success(l) && classify_conf(l))
					return 1;
				return 0;
			}
		}
	}
	return 0;
}

int per_event_processing(llist *l)
{
	int rc;

	switch (report_detail)
	{
		case D_SUM:
			rc = per_event_summary(l);
			break;
		case D_DETAILED:
			rc = per_event_detailed(l);
			break;
		case D_SPECIFIC:
		default:
			rc = 0;
			break;
	} 
	return rc;
}

static int per_event_summary(llist *l)
{
	int rc = 0;

	switch (report_type)
	{
		case RPT_SUMMARY:
			do_summary_total(l);
			rc = 1;
			break;
		case RPT_AVC:
			if (list_find_msg(l, AUDIT_AVC)) {
				if (alist_find_avc(l->s.avc)) {
					do { 
						slist_add_if_uniq(&sd.avc_objs,
						      l->s.avc->cur->tcontext);
					} while (alist_next_avc(l->s.avc));
				}
			} else {
				if (list_find_msg(l, AUDIT_USER_AVC)) {
					if (alist_find_avc(l->s.avc)) { 
						do {
							slist_add_if_uniq(
								&sd.avc_objs,
						    l->s.avc->cur->tcontext);
						} while (alist_next_avc(
								l->s.avc));
					}
				}
			}
			break;
		case RPT_MAC:
			if (list_find_msg_range(l, AUDIT_MAC_POLICY_LOAD,
						AUDIT_MAC_MAP_DEL)) {
				ilist_add_if_uniq(&sd.mac_list, 
							l->head->type, 0);
			} else {
				if (list_find_msg_range(l, 
					AUDIT_FIRST_USER_LSPP_MSG,
						AUDIT_LAST_USER_LSPP_MSG)) {
					ilist_add_if_uniq(&sd.mac_list, 
							l->head->type, 0);
				}
			}
			break;
		case RPT_CONFIG:
			UNIMPLEMENTED;
			break;
		case RPT_AUTH:
			if (list_find_msg(l, AUDIT_USER_AUTH)) {
				if (l->s.loginuid == -1 && l->s.acct != NULL)
					slist_add_if_uniq(&sd.users, l->s.acct);
				else {
					char name[64];

					slist_add_if_uniq(&sd.users,
						aulookup_uid(l->s.loginuid,
							name,
							sizeof(name))
						);
				}
			} else if (list_find_msg(l, AUDIT_USER_ACCT)) {
				// Only count the failures
				if (l->s.success == S_FAILED) {
					if (l->s.loginuid == -1 && 
						l->s.acct != NULL)
					slist_add_if_uniq(&sd.users, l->s.acct);
					else {
						char name[64];
	
						slist_add_if_uniq(&sd.users,
							aulookup_uid(
								l->s.loginuid,
								name,
								sizeof(name))
							);
					}
				}
			}
			break;
		case RPT_LOGIN:
			if (list_find_msg(l, AUDIT_USER_LOGIN)) {
				if (l->s.loginuid == -1 && l->s.acct != NULL)
					slist_add_if_uniq(&sd.users, l->s.acct);
				else {
					char name[64];

					slist_add_if_uniq(&sd.users,
						aulookup_uid(l->s.loginuid,
							name,
							sizeof(name))
						);
				}
			}
			break;
		case RPT_ACCT_MOD:
			UNIMPLEMENTED;
			break;
		case RPT_EVENT: /* We will borrow the pid list */
			if (l->head->type != -1) {
				ilist_add_if_uniq(&sd.pids, l->head->type, 0);
			}
			break;
		case RPT_FILE:
			if (l->s.filename) {
				const snode *sn;
				slist *sptr = l->s.filename;

				slist_first(sptr);
				sn=slist_get_cur(sptr);
				while (sn) {
					if (sn->str)
						slist_add_if_uniq(&sd.files,
								sn->str);
					sn=slist_next(sptr);
				} 
			}
			break;
		case RPT_HOST:
			if (l->s.hostname)
				slist_add_if_uniq(&sd.hosts, l->s.hostname);
			break;
		case RPT_PID:
			if (l->s.pid != -1) {
				ilist_add_if_uniq(&sd.pids, l->s.pid, 0);
			}
			break;
		case RPT_SYSCALL:
			if (l->s.syscall > 0) {
				ilist_add_if_uniq(&sd.sys_list,
						l->s.syscall, l->s.arch);
			}
			break;
		case RPT_TERM:
			if (l->s.terminal)
				slist_add_if_uniq(&sd.terms, l->s.terminal);
			break;
		case RPT_USER:
			if (l->s.loginuid != -1) {
				char tmp[32];
				snprintf(tmp, sizeof(tmp), "%d", l->s.loginuid);
				slist_add_if_uniq(&sd.users, tmp);
			}
			break;
		case RPT_EXE:
			if (l->s.exe)
				slist_add_if_uniq(&sd.exes, l->s.exe);
			break;
		case RPT_ANOMALY:
			if (list_find_msg_range(l, AUDIT_FIRST_ANOM_MSG,
							AUDIT_LAST_ANOM_MSG)) {
				ilist_add_if_uniq(&sd.anom_list, 
							l->head->type, 0);
			} else {
				if (list_find_msg_range(l, 
					AUDIT_FIRST_KERN_ANOM_MSG,
						AUDIT_LAST_KERN_ANOM_MSG)) {
					ilist_add_if_uniq(&sd.anom_list, 
							l->head->type, 0);
				}
			}
			break;
		case RPT_RESPONSE:
			if (list_find_msg_range(l, AUDIT_FIRST_ANOM_RESP,
							AUDIT_LAST_ANOM_RESP)) {
				ilist_add_if_uniq(&sd.resp_list, 
							l->head->type, 0);
			}
			break;
		case RPT_CRYPTO:
			if (list_find_msg_range(l, AUDIT_FIRST_KERN_CRYPTO_MSG,
						AUDIT_LAST_KERN_CRYPTO_MSG)) {
				ilist_add_if_uniq(&sd.crypto_list, 
							l->head->type, 0);
			} else {
				if (list_find_msg_range(l, 
					AUDIT_FIRST_CRYPTO_MSG,
						AUDIT_LAST_CRYPTO_MSG)) {
					ilist_add_if_uniq(&sd.crypto_list, 
							l->head->type, 0);
				}
			}
			break;
		case RPT_KEY:
			if (l->s.key) {
				const snode *sn;
				slist *sptr = l->s.key;

				slist_first(sptr);
				sn=slist_get_cur(sptr);
				while (sn) {
					if (sn->str &&
						    strcmp(sn->str, "(null)"))
						slist_add_if_uniq(&sd.keys,
								sn->str);
					sn=slist_next(sptr);
				} 
			}
			break;
		default:
			break;
	}
	return rc;
}

static int per_event_detailed(llist *l)
{
	int rc = 0;

	switch (report_type)
	{
		case RPT_AVC:
			if (list_find_msg(l, AUDIT_AVC)) {
				print_per_event_item(l);
				rc = 1;
			} else if (list_find_msg(l, AUDIT_USER_AVC)) {
				print_per_event_item(l);
				rc = 1;
			}
			break;
		case RPT_MAC:
			if (report_detail == D_DETAILED) {
				if (list_find_msg_range(l, 
						AUDIT_MAC_POLICY_LOAD,
						AUDIT_MAC_UNLBL_STCDEL)) {
					print_per_event_item(l);
					rc = 1;
				} else {
					if (list_find_msg_range(l, 
						AUDIT_FIRST_USER_LSPP_MSG,
						AUDIT_LAST_USER_LSPP_MSG)) {
						print_per_event_item(l);
						rc = 1;
					}
				}
			}
			break;
		case RPT_CONFIG:
			if (list_find_msg(l, AUDIT_CONFIG_CHANGE)) {
				print_per_event_item(l);
				rc = 1;
			} else if (list_find_msg(l, AUDIT_DAEMON_CONFIG)) {
				print_per_event_item(l);
				rc = 1;
			} else if (list_find_msg(l, AUDIT_USYS_CONFIG)) {
				print_per_event_item(l);
				rc = 1;
			} else if (list_find_msg_range(l,
					AUDIT_MAC_POLICY_LOAD,
					AUDIT_MAC_UNLBL_STCDEL)) {
						print_per_event_item(l);
						rc = 1;
			}
			break;
		case RPT_AUTH:
			if (list_find_msg(l, AUDIT_USER_AUTH)) {
				print_per_event_item(l);
				rc = 1;
			} else if (list_find_msg(l, AUDIT_USER_ACCT)) {
				// Only count the failed acct
				if (l->s.success == S_FAILED) {
					print_per_event_item(l);
					rc = 1;
				}
			}
			break;
		case RPT_LOGIN:
			if (list_find_msg(l, AUDIT_USER_LOGIN)) {
				print_per_event_item(l);
				rc = 1;
			}
			break;
		case RPT_ACCT_MOD:
			if (list_find_msg(l, AUDIT_USER_CHAUTHTOK)) {
				print_per_event_item(l);
				rc = 1;
			} else if (list_find_msg_range(l,
					AUDIT_ADD_USER, AUDIT_DEL_GROUP)) {
				print_per_event_item(l);
				rc = 1;
			} else if (list_find_msg(l, AUDIT_CHGRP_ID)) {
				print_per_event_item(l);
				rc = 1;
			} else if (list_find_msg_range(l,
					AUDIT_ROLE_ASSIGN,
					AUDIT_ROLE_REMOVE)) {
				print_per_event_item(l);
				rc = 1;
			}
			break;
		case RPT_EVENT:
			list_first(l);
			if (report_detail == D_DETAILED) {
				print_per_event_item(l);
				rc = 1;
			} else { //  specific event report
				UNIMPLEMENTED;
			}
			break;
		case RPT_FILE:
			list_first(l);
			if (report_detail == D_DETAILED) {
				if (l->s.filename) {
					print_per_event_item(l);
					rc = 1;
				}
			} else { //  specific file report
				UNIMPLEMENTED;
			}
			break;
		case RPT_HOST:
			list_first(l);
			if (report_detail == D_DETAILED) {
				if (l->s.hostname) {
					print_per_event_item(l);
					rc = 1;
				}
			} else { //  specific host report
				UNIMPLEMENTED;
			}
			break;
		case RPT_PID:
			list_first(l);
			if (report_detail == D_DETAILED) {
				if (l->s.pid >= 0) {
					print_per_event_item(l);
					rc = 1;
				}
			} else { //  specific pid report
				UNIMPLEMENTED;
			}
			break;
		case RPT_SYSCALL:
			list_first(l);
			if (report_detail == D_DETAILED) {
				if (l->s.syscall) {
					print_per_event_item(l);
					rc = 1;
				}
			} else { //  specific syscall report
				UNIMPLEMENTED;
			}
			break;
		case RPT_TERM:
			list_first(l);
			if (report_detail == D_DETAILED) {
				if (l->s.terminal) {
					print_per_event_item(l);
					rc = 1;
				}
			} else { //  specific terminal report
				UNIMPLEMENTED;
			}
			break;
		case RPT_USER:
			list_first(l);
			if (report_detail == D_DETAILED) {
				if (l->s.uid != -1) {
					print_per_event_item(l);
					rc = 1;
				}
			} else { //  specific user report
				UNIMPLEMENTED;
			}
			break;
		case RPT_EXE:
			list_first(l);
			if (report_detail == D_DETAILED) {
				if (l->s.exe) {
					print_per_event_item(l);
					rc = 1;
				}
			} else { //  specific exe report
				UNIMPLEMENTED;
			}
			break;
		case RPT_ANOMALY:
			if (report_detail == D_DETAILED) {
				if (list_find_msg_range(l, 
						AUDIT_FIRST_ANOM_MSG,
						AUDIT_LAST_ANOM_MSG)) {
					print_per_event_item(l);
					rc = 1;
				} else {
					if (list_find_msg_range(l, 
						AUDIT_FIRST_KERN_ANOM_MSG,
						AUDIT_LAST_KERN_ANOM_MSG)) {
						print_per_event_item(l);
						rc = 1;
					}
				}
			} else { // FIXME: specific anom report
				UNIMPLEMENTED;
			}
			break;
		case RPT_RESPONSE:
			if (report_detail == D_DETAILED) {
				if (list_find_msg_range(l,	
						AUDIT_FIRST_ANOM_RESP,
						AUDIT_LAST_ANOM_RESP)) {
					print_per_event_item(l);
					rc = 1;
				}
			} else { // FIXME: specific resp report
				UNIMPLEMENTED;
			}
			break;
		case RPT_CRYPTO:
			if (report_detail == D_DETAILED) {
				if (list_find_msg_range(l, 
						AUDIT_FIRST_KERN_CRYPTO_MSG,
						AUDIT_LAST_KERN_CRYPTO_MSG)) {
					print_per_event_item(l);
					rc = 1;
				} else {
					if (list_find_msg_range(l, 
						AUDIT_FIRST_CRYPTO_MSG,
						AUDIT_LAST_CRYPTO_MSG)) {
						print_per_event_item(l);
						rc = 1;
					}
				}
			} else { // FIXME: specific crypto report
				UNIMPLEMENTED;
			}
			break;
		case RPT_KEY:
			list_first(l);
			if (report_detail == D_DETAILED) {
				if (l->s.key) {
					slist_first(l->s.key);
					if (strcmp(l->s.key->cur->str,
							"(null)")) {
						print_per_event_item(l);
						rc = 1;
					}
				}
			} else { //  specific key report
				UNIMPLEMENTED;
			}
			break;
		default:
			break;
	}
	return rc;
}

static void do_summary_total(llist *l)
{
	// add events
	sd.events++;

	// add config changes
	if (list_find_msg(l, AUDIT_CONFIG_CHANGE))
		sd.changes++;
	if (list_find_msg(l, AUDIT_DAEMON_CONFIG)) 
		sd.changes++;
	if (list_find_msg(l, AUDIT_USYS_CONFIG)) 
		sd.changes++;
	list_first(l);
	if (list_find_msg_range(l, AUDIT_MAC_POLICY_LOAD,
					AUDIT_MAC_UNLBL_STCDEL))
		sd.changes++;

	// add acct changes
	if (list_find_msg(l, AUDIT_USER_CHAUTHTOK))
		sd.acct_changes++;
	if (list_find_msg_range(l, AUDIT_ADD_USER, AUDIT_DEL_GROUP))
		sd.acct_changes++;
	if (list_find_msg(l, AUDIT_CHGRP_ID))
		sd.acct_changes++;
	list_first(l);
	if (list_find_msg_range(l, AUDIT_ROLE_ASSIGN, AUDIT_ROLE_REMOVE))
		sd.acct_changes++;

	// Crypto
	list_first(l);
	if (list_find_msg_range(l, AUDIT_FIRST_KERN_CRYPTO_MSG,
					AUDIT_LAST_KERN_CRYPTO_MSG))
		sd.crypto++;
	if (list_find_msg_range(l, AUDIT_FIRST_CRYPTO_MSG, 
					AUDIT_LAST_CRYPTO_MSG))
		sd.crypto++;

	// add logins
	if (list_find_msg(l, AUDIT_USER_LOGIN)) {
		if (l->s.success == S_SUCCESS)
			sd.good_logins++;
		else if (l->s.success == S_FAILED)
			sd.bad_logins++;
	}

	// add use of auth
	if (list_find_msg(l, AUDIT_USER_AUTH)) {
		if (l->s.success == S_SUCCESS)
			sd.good_auth++;
		else if (l->s.success == S_FAILED)
			sd.bad_auth++;
	} else if (list_find_msg(l, AUDIT_USER_ACCT)) {
		// Only count the failures
		if (l->s.success == S_FAILED)
			sd.bad_auth++;
	} else if (list_find_msg(l, AUDIT_GRP_AUTH)) {
		if (l->s.success == S_SUCCESS)
			sd.good_auth++;
		else if (l->s.success == S_FAILED)
			sd.bad_auth++;
	}

	// add users
	if (l->s.loginuid != -1) {
		char tmp[32];
		snprintf(tmp, sizeof(tmp), "%d", l->s.loginuid);
		slist_add_if_uniq(&sd.users, tmp);
	}

	// add terminals
	if (l->s.terminal)
		slist_add_if_uniq(&sd.terms, l->s.terminal);

	// add hosts
	if (l->s.hostname)
		slist_add_if_uniq(&sd.hosts, l->s.hostname);

	// add execs
	if (l->s.exe)
		slist_add_if_uniq(&sd.exes, l->s.exe);

	// add files
	if (l->s.filename) {
		const snode *sn;
		slist *sptr = l->s.filename;

		slist_first(sptr);
		sn=slist_get_cur(sptr);
		while (sn) {
			if (sn->str)
				slist_add_if_uniq(&sd.files, sn->str);
			sn=slist_next(sptr);
		} 
	}

	// add avcs
	if (list_find_msg(l, AUDIT_AVC)) 
		sd.avcs++;
	else if (list_find_msg(l, AUDIT_USER_AVC))
			sd.avcs++;

	// MAC
	list_first(l);
	if (list_find_msg_range(l, AUDIT_MAC_POLICY_LOAD,
					AUDIT_MAC_UNLBL_STCDEL))
		sd.mac++;
	if (list_find_msg_range(l, AUDIT_FIRST_USER_LSPP_MSG, 
					AUDIT_LAST_USER_LSPP_MSG))
		sd.mac++;

	// add failed syscalls
	if (l->s.success == S_FAILED && l->s.syscall > 0)
		sd.failed_syscalls++;

	// add pids
	if (l->s.pid != -1) {
		ilist_add_if_uniq(&sd.pids, l->s.pid, 0);
	}

	// add anomalies
	if (list_find_msg_range(l, AUDIT_FIRST_ANOM_MSG, AUDIT_LAST_ANOM_MSG))
		sd.anomalies++;
	if (list_find_msg_range(l, AUDIT_FIRST_KERN_ANOM_MSG,
				 AUDIT_LAST_KERN_ANOM_MSG))
		sd.anomalies++;

	// add response to anomalies
	if (list_find_msg_range(l, AUDIT_FIRST_ANOM_RESP, AUDIT_LAST_ANOM_RESP))
		sd.responses++;

	// add keys
	if (l->s.key) {
		const snode *sn;
		slist *sptr = l->s.key;

		slist_first(sptr);
		sn=slist_get_cur(sptr);
		while (sn) {
			if (sn->str && strcmp(sn->str, "(null)")) {
				slist_add_if_uniq(&sd.keys, sn->str);
			}
			sn=slist_next(sptr);
		} 
	}
}

