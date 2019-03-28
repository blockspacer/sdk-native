#ifndef TANKER_SDK_TANKER_GROUPS_H
#define TANKER_SDK_TANKER_GROUPS_H

#include <stdint.h>

#include <tanker/async.h>
#include <tanker/base64.h>
#include <tanker/tanker.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Create a group containing the given users.
 * Share a symetric key of an encrypted data with other users.
 *
 * \param session A tanker tanker_t* instance.
 * \pre tanker_status == TANKER_STATUS_OPEN
 * \param member_uids Array of strings describing the group members.
 * \param nb_members The number of members in member_uids.
 *
 * \return A future of the group ID as a string.
 * \throws TANKER_ERROR_USER_NOT_FOUND One of the members was not found, no
 * action was done
 * \throws TANKER_ERROR_INVALID_GROUP_SIZE The group is either empty, or has too
 * many members
 */
tanker_future_t* tanker_create_group(tanker_t* session,
                                     char const* const* member_uids,
                                     uint64_t nb_members);

/*!
 * Updates an existing group, referenced by its groupId,
 * adding the user identified by their user Ids to the group's members.
 *
 * \param session A tanker tanker_t* instance.
 * \pre tanker_status == TANKER_STATUS_OPEN
 * \param group_id The group ID returned by tanker_create_group
 * \param users_to_add Array of strings describing the new group members.
 * \param nb_users_to_add The number of users in users_to_add.
 *
 * \return An empty future.
 * \throws TANKER_ERROR_USER_NOT_FOUND One of the users was not found, no
 * action was done
 * \throws TANKER_ERROR_INVALID_GROUP_SIZE Too many users were added to the
 * group.
 */
tanker_future_t* tanker_update_group_members(tanker_t* session,
                                             char const* group_id,
                                             char const* const* users_to_add,
                                             uint64_t nb_users_to_add);

#ifdef __cplusplus
}
#endif

#endif // TANKER_SDK_TANKER_GROUPS_H