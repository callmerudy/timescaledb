#ifndef TIMESCALEDB_EVENT_TRIGGER_H
#define TIMESCALEDB_EVENT_TRIGGER_H

#include <postgres.h>
#include <nodes/pg_list.h>

typedef struct EventTriggerDroppedObject
{
	Oid			classid;
	Oid			objectid;
	char	   *schema;
	char	   *objname;
	char	   *objtype;
	List	   *addrnames;
} EventTriggerDroppedObject;

extern List *event_trigger_dropped_objects(void);
extern List *event_trigger_ddl_commands(void);
extern void _event_trigger_init(void);
extern void _event_trigger_fini(void);

#endif							/* TIMESCALEDB_EVENT_TRIGGER_H */
