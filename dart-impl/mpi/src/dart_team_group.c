/**
 *  \file dart_team_group.c
 *
 *  Implementation of dart operations on team_group.
 */

#include <mpi.h>
#include <dash/dart/base/logging.h>
#include <dash/dart/if/dart_team_group.h>
#include <dash/dart/if/dart_initialization.h>
#include <dash/dart/if/dart_types.h>
#include <dash/dart/mpi/dart_team_private.h>
#include <dash/dart/mpi/dart_translation.h>
#include <dash/dart/mpi/dart_group_priv.h>

dart_ret_t dart_group_init(
  dart_group_t *group)
{
  group -> mpi_group = MPI_GROUP_EMPTY;
  return DART_OK;
}

dart_ret_t dart_group_fini(
  dart_group_t *group)
{
  group -> mpi_group = MPI_GROUP_NULL;
  return DART_OK;
}

dart_ret_t dart_group_copy(
  const dart_group_t *gin,
  dart_group_t *gout)
{
  gout -> mpi_group = gin -> mpi_group;
  return DART_OK;
}

#if 0
dart_ret_t dart_adapt_group_union(
  const dart_group_t *g1,
  const dart_group_t *g2,
  dart_group_t *gout)
{
  return MPI_Group_union(
    g1 -> mpi_group,
    g2 -> mpi_group,
    &(gout -> mpi_group));
}
#endif

dart_ret_t dart_group_union(
  const dart_group_t *g1,
  const dart_group_t *g2,
  dart_group_t *gout)
{
  /* g1 and g2 are both ordered groups. */
  int ret = MPI_Group_union(
              g1->mpi_group,
              g2->mpi_group,
              &(gout -> mpi_group));
  if (ret == MPI_SUCCESS) {
    int i, j, k, size_in, size_out;
    dart_unit_t *pre_unitidsout, *post_unitidsout;;

    MPI_Group group_all;
    MPI_Comm_group(MPI_COMM_WORLD, &group_all);
    MPI_Group_size(gout->mpi_group, &size_out);
    if (size_out > 1) {
      MPI_Group_size(g1->mpi_group, &size_in);
      pre_unitidsout  = (dart_unit_t *)malloc(
                          size_out * sizeof (dart_unit_t));
      post_unitidsout = (dart_unit_t *)malloc(
                          size_out * sizeof (dart_unit_t));
      dart_group_getmembers (gout, pre_unitidsout);

      /* Sort gout by the method of 'merge sort'. */
      i = k = 0;
      j = size_in;

      while ((i <= size_in - 1) && (j <= size_out - 1)) {
        post_unitidsout[k++] =
          (pre_unitidsout[i] <= pre_unitidsout[j])
          ? pre_unitidsout[i++]
          : pre_unitidsout[j++];
      }
      while (i <= size_in -1) {
        post_unitidsout[k++] = pre_unitidsout[i++];
      }
      while (j <= size_out -1) {
        post_unitidsout[k++] = pre_unitidsout[j++];
      }
      gout -> mpi_group = MPI_GROUP_EMPTY;
      MPI_Group_incl(
        group_all,
        size_out,
        post_unitidsout,
        &(gout->mpi_group));
      free (pre_unitidsout);
      free (post_unitidsout);
    }
    ret = DART_OK;
  }
  return ret;
}

dart_ret_t dart_group_intersect(
  const dart_group_t *g1,
  const dart_group_t *g2,
  dart_group_t *gout)
{
  return MPI_Group_intersection(
           g1 -> mpi_group,
           g2 -> mpi_group,
           &(gout -> mpi_group));
}

dart_ret_t dart_group_addmember(
  dart_group_t *g,
  dart_unit_t unitid)
{
  int array[1];
  dart_group_t* group_copy, *group;
  MPI_Group     newgroup, group_all;
  /* Group_all comprises all the running units. */
  MPI_Comm_group(MPI_COMM_WORLD, &group_all);
  group_copy = (dart_group_t*)malloc(sizeof(dart_group_t));
  group      = (dart_group_t*)malloc(sizeof(dart_group_t));
  dart_group_copy(g, group_copy);
  array[0]   = unitid;
  MPI_Group_incl(group_all, 1, array, &newgroup);
  group->mpi_group = newgroup;
  /* Make the new group being an ordered group. */
  dart_group_union (group_copy, group, g);
  free (group_copy);
  free (group);
  return DART_OK;
}

dart_ret_t dart_group_delmember(
  dart_group_t *g,
  dart_unit_t unitid)
{
  int array[1];
  MPI_Group newgroup, group_all;
  MPI_Comm_group(MPI_COMM_WORLD, &group_all);
  array[0] = unitid;
  MPI_Group_incl(
    group_all,
    1,
    array,
    &newgroup);
  MPI_Group_difference(
    g -> mpi_group,
    newgroup,
    &(g -> mpi_group));
  return DART_OK;
}

dart_ret_t dart_group_size(
  const dart_group_t *g,
  size_t *size)
{
  int s;
  MPI_Group_size (g -> mpi_group, &s);
  (*size) = s;
  return DART_OK;
}

dart_ret_t dart_group_getmembers(
  const dart_group_t *g,
  dart_unit_t *unitids)
{
  int size, i;
  int *array;
  MPI_Group group_all;
  MPI_Group_size(g -> mpi_group, &size);
  MPI_Comm_group(MPI_COMM_WORLD, &group_all);
  array = (int*) malloc (sizeof (int) * size);
  for (i = 0; i < size; i++) {
    array[i] = i;
  }
  MPI_Group_translate_ranks(
    g->mpi_group,
    size,
    array,
    group_all,
    unitids);
  free (array);
  return DART_OK;
}

dart_ret_t dart_group_split(
  const dart_group_t *g,
  size_t n,
  dart_group_t **gout)
{
  MPI_Group grouptem;
  int size, length, i, ranges[1][3];

  MPI_Group_size (g -> mpi_group, &size);

  /* Ceiling division. */
  length = (size+(int)n-1)/(int)n;

  /* Note: split the group into chunks of subgroups. */
  for (i = 0; i < (int)n; i++)
  {
    if (i * length < size)
    {
      ranges[0][0] = i * length;

      if (i * length + length <= size)
      {
        ranges[0][1] = i * length + length -1;
      }
      else
      {
        ranges[0][1] = size - 1;
      }

      ranges[0][2] = 1;
      MPI_Group_range_incl(
        g -> mpi_group,
        1,
        ranges,
        &grouptem);
      (*(gout + i))->mpi_group = grouptem;
    }
    else
    {
      (*(gout + i))->mpi_group = MPI_GROUP_EMPTY;
    }
  }
  return DART_OK;
}

dart_ret_t dart_group_sizeof (size_t *size)
{
  *size = sizeof (dart_group_t);
  return DART_OK;
}

dart_ret_t dart_group_ismember(
  const dart_group_t *g,
  dart_unit_t unitid,
  int32_t *ismember)
{
  dart_unit_t id;
  dart_myid (&id);

  int           i, size;
  dart_unit_t*  ranks;

  MPI_Group_size(g->mpi_group, &size);
  ranks = (dart_unit_t *)malloc(size * sizeof(dart_unit_t));
  dart_group_getmembers (g, ranks);
  for (i = 0; i < size; i++) {
    if (ranks[i] == unitid) {
      break;
    }
  }
  *ismember = (i!=size);
  DART_LOG_DEBUG("%2d: GROUP_ISMEMBER - %s", unitid, (*ismember) ? "yes" : "no");
  return DART_OK;
}

dart_ret_t dart_team_get_group(
  dart_team_t teamid,
  dart_group_t *group)
{
  MPI_Comm comm;
  uint16_t index;

  int result = dart_adapt_teamlist_convert(teamid, &index);
  if (result == -1) {
    return DART_ERR_INVAL;
  }
  comm = dart_teams[index];
  MPI_Comm_group (comm, & (group->mpi_group));
  return DART_OK;
}

/**
 *  TODO: Differentiate units belonging to team_id and that not
 *  belonging to team_id
 *  within the function or outside it?
 *  FIX: Outside it.
 *
 *  The teamid stands for a superteam related to the new generated
 *  newteam.
 */
dart_ret_t dart_team_create(
  dart_team_t teamid,
  const dart_group_t* group,
  dart_team_t *newteam)
{
  MPI_Comm comm;
  MPI_Comm subcomm;
  MPI_Win win;
  uint16_t index, unique_id;
  size_t size;
  dart_team_t max_teamid = -1;
  dart_unit_t sub_unit, unit;

  dart_myid (&unit);
  dart_size (&size);
  dart_team_myid (teamid, &sub_unit);

  int result = dart_adapt_teamlist_convert (teamid, &unique_id);
  if (result == -1) {
    return DART_ERR_INVAL;
  }
  comm = dart_teams[unique_id];
  subcomm = MPI_COMM_NULL;

  MPI_Comm_create (comm, group -> mpi_group, &subcomm);

  *newteam = DART_TEAM_NULL;

  /* Get the maximum next_availteamid among all the units belonging to
   * the parent team specified by 'teamid'. */
  MPI_Allreduce(
    &dart_next_availteamid,
    &max_teamid,
    1,
    MPI_INT32_T,
    MPI_MAX,
    comm);
  dart_next_availteamid = max_teamid + 1;

  if (subcomm != MPI_COMM_NULL) {
    int result = dart_adapt_teamlist_alloc(max_teamid, &index);
    if (result == -1) {
      return DART_ERR_OTHER;
    }
    /* max_teamid is thought to be the new created team ID. */
    *newteam = max_teamid;
    dart_teams[index] = subcomm;
    MPI_Win_create_dynamic(MPI_INFO_NULL, subcomm, &win);
    dart_win_lists[index] = win;
  }
#if 0
  /* Another way of generating the available teamID for the newly crated team. */
  if (subcomm != MPI_COMM_NULL)
  {
    /* Get the maximum next_availteamid among all the units belonging to the
     * created sub-communicator. */
    MPI_Allreduce (&next_availteamid, &max_teamid, 1, MPI_INT, MPI_MAX, subcomm);
    int result = dart_adapt_teamlist_alloc (max_teamid, &index);

    if (result == -1)
    {
      return DART_ERR_OTHER;
    }

    *newteam = max_teamid;
    teams[index] = subcomm;
    MPI_Comm_rank (subcomm, &rank);

    if (rank == 0)
    {
      root = sub_unit;
      if (sub_unit != 0)
      {
        MPI_Send (&root, 1, MPI_INT, 0, 0, comm);
      }
    }

    next_availteamid = max_teamid + 1;
  }

  if (sub_unit == 0)
  {
    if (root == -1)
    {
      MPI_Recv (&root, 1, MPI_INT, MPI_ANY_SOURCE, 0, comm, MPI_STATUS_IGNORE);
    }
  }

  MPI_Bcast (&root, 1, MPI_INT, 0, comm);

  /* Broadcast the calculated max_teamid to all the units not belonging to the
   * sub-communicator. */
  MPI_Bcast (&max_teamid, 1, MPI_INT, root, comm);
  if (subcomm == MPI_COMM_NULL)
  {
    /* 'Next_availteamid' is changed iff it is smaller than 'max_teamid + 1' */
    if (max_teamid + 1 > next_availteamid)
    {
      next_availteamid = max_teamid + 1;
    }
  }
#endif

  if (subcomm != MPI_COMM_NULL) {
#if !defined(DART_MPI_DISABLE_SHARED_WINDOWS)
    int    i;
    size_t n;

    MPI_Comm sharedmem_comm;
    MPI_Group sharedmem_group, group_all;
    MPI_Comm_split_type(
      subcomm,
      MPI_COMM_TYPE_SHARED,
      1,
      MPI_INFO_NULL,
      &sharedmem_comm);
    dart_sharedmem_comm_list[index] = sharedmem_comm;
    if (sharedmem_comm != MPI_COMM_NULL) {
      MPI_Comm_size(
        sharedmem_comm,
        &(dart_sharedmemnode_size[index]));

  //    dart_unit_mapping[index] = (int*)malloc (
  //      dart_sharedmem_size[index] * sizeof (int));

      MPI_Comm_group(sharedmem_comm, &sharedmem_group);
      MPI_Comm_group(MPI_COMM_WORLD, &group_all);

      int* dart_unit_mapping = (int *)malloc (
        dart_sharedmemnode_size[index] * sizeof (int));
      int* sharedmem_ranks = (int*)malloc (
        dart_sharedmemnode_size[index] * sizeof (int));

      dart_sharedmem_table[index] = (int*)malloc(size * sizeof(int));

      for (i = 0; i < dart_sharedmemnode_size[index]; i++) {
        sharedmem_ranks[i] = i;
      }

  //    MPI_Group_translate_ranks (sharedmem_group, dart_sharedmem_size[index],
  //        sharedmem_ranks, group_all, dart_unit_mapping[index]);
      MPI_Group_translate_ranks(
        sharedmem_group,
        dart_sharedmemnode_size[index],
        sharedmem_ranks,
        group_all,
        dart_unit_mapping);

      for (n = 0; n < size; n++) {
        dart_sharedmem_table[index][n] = -1;
      }
      for (i = 0; i < dart_sharedmemnode_size[index]; i++) {
        dart_sharedmem_table[index][dart_unit_mapping[i]] = i;
      }
      free (sharedmem_ranks);
      free (dart_unit_mapping);
    }

#endif
    MPI_Win_lock_all(0, win);
    DART_LOG_DEBUG ("%2d: TEAMCREATE  - create team %d out of parent team %d",
           unit, *newteam, teamid);
  }
  return DART_OK;
}

dart_ret_t dart_team_destroy(
  dart_team_t teamid)
{
  MPI_Comm comm;
  MPI_Win win;
  uint16_t index;
  dart_unit_t id;

  int result = dart_adapt_teamlist_convert(teamid, &index);
  if (result == -1) {
    return DART_ERR_INVAL;
  }
  comm = dart_teams[index];

  dart_myid (&id);

//  free (dart_unit_mapping[index]);

//  MPI_Win_free (&(sharedmem_win_list[index]));
#if !defined(DART_MPI_DISABLE_SHARED_WINDOWS)
  free(dart_sharedmem_table[index]);
#endif
  win = dart_win_lists[index];
  MPI_Win_unlock_all(win);
  MPI_Win_free(&win);
  dart_adapt_teamlist_recycle(index, result);

  /* -- Release the communicator associated with teamid -- */
  MPI_Comm_free (&comm);

  DART_LOG_DEBUG("%2d: TEAMDESTROY  - destroy team %d", id, teamid);
  return DART_OK;
}

dart_ret_t dart_myid(dart_unit_t *unitid)
{
  if (dart_initialized()) {
    MPI_Comm_rank(MPI_COMM_WORLD, unitid);
  } else {
    *unitid = -1;
  }
  return DART_OK;
}

dart_ret_t dart_size(size_t *size)
{
  int s;
  MPI_Comm_size (MPI_COMM_WORLD, &s);
  (*size) = s;
  return DART_OK;
}

dart_ret_t dart_team_myid(
  dart_team_t teamid,
  dart_unit_t *unitid)
{
  MPI_Comm comm;
  uint16_t index;
  int result = dart_adapt_teamlist_convert(teamid, &index);
  if (result == -1)
  {
    return DART_ERR_INVAL;
  }
  comm = dart_teams[index];
  MPI_Comm_rank (comm, unitid);

  return DART_OK;
}

dart_ret_t dart_team_size(
  dart_team_t teamid,
  size_t *size)
{
  MPI_Comm comm;
  uint16_t index;
  if (teamid == DART_TEAM_NULL) {
    return DART_ERR_INVAL;
  }
  int result = dart_adapt_teamlist_convert(teamid, &index);
  if (result == -1) {
    return DART_ERR_INVAL;
  }
  comm = dart_teams[index];

  int s;
  MPI_Comm_size (comm, &s);
  (*size) = s;
  return DART_OK;
}

dart_ret_t dart_team_unit_l2g(
  dart_team_t teamid,
  dart_unit_t localid,
  dart_unit_t *globalid)
{
#if 0
  dart_unit_t *unitids;
  int size;
  int i = 0;
  dart_group_t group;
  dart_team_get_group (teamid, &group);
  MPI_Group_size (group.mpi_group, &size);
  if (localid >= size)
  {
    DART_LOG_ERROR ("Invalid localid input");
    return DART_ERR_INVAL;
  }
  unitids = (dart_unit_t*)malloc (sizeof(dart_unit_t) * size);
  dart_group_getmembers (&group, unitids);

  /* The unitids array is arranged in ascending order. */
  *globalid = unitids[localid];
//  printf ("globalid is %d\n", *globalid);
  return DART_OK;
#endif
  int size;
  dart_group_t group;

  dart_team_get_group (teamid, &group);
  MPI_Group_size (group.mpi_group, &size);

  if (localid >= size) {
    DART_LOG_ERROR ("Invalid localid input: %d", localid);
    return DART_ERR_INVAL;
  }
  if (teamid == DART_TEAM_ALL) {
    *globalid = localid;
  }
  else {
    MPI_Group group_all;
    MPI_Comm_group(MPI_COMM_WORLD, &group_all);
    MPI_Group_translate_ranks(
      group.mpi_group,
      1,
      &localid,
      group_all,
      globalid);
  }

  return DART_OK;
}

dart_ret_t dart_team_unit_g2l(
  dart_team_t teamid,
  dart_unit_t globalid,
  dart_unit_t *localid)
{
#if 0
  dart_unit_t *unitids;
  int size;
  int i;
  dart_group_t group;
  dart_team_get_group (teamid, &group);
  MPI_Group_size (group.mpi_group, &size);
  unitids = (dart_unit_t *)malloc (sizeof (dart_unit_t) * size);

  dart_group_getmembers (&group, unitids);


  for (i = 0; (i < size) && (unitids[i] < globalid); i++);

  if ((i == size) || (unitids[i] > globalid))
  {
    *localid = -1;
    return DART_OK;
  }

  *localid = i;
  return DART_OK;
#endif
  if(teamid == DART_TEAM_ALL) {
    *localid = globalid;
  }
  else {
    dart_group_t group;
    MPI_Group group_all;
    dart_team_get_group(teamid, &group);
    MPI_Comm_group(MPI_COMM_WORLD, &group_all);
    MPI_Group_translate_ranks(
      group_all,
      1,
      &globalid,
      group.mpi_group,
      localid);
  }
  return DART_OK;
}
