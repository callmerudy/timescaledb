#include <postgres.h>
#include <commands/event_trigger.h>
#include <utils/builtins.h>
#include <executor/executor.h>
#include <access/htup_details.h>
#include <catalog/pg_type.h>

#include "event_trigger.h"

#define DDL_INFO_NATTS 9
#define DROPPED_OBJECTS_NATTS 12

/* Function manager info for the event "pg_event_trigger_ddl_commands", which is
 * used to retrieve information on executed DDL commands in an event
 * trigger. The function manager info is initialized on extension load. */
static FmgrInfo ddl_commands_fmgrinfo;
static FmgrInfo dropped_objects_fmgrinfo;

/*
 * Get a list of executed DDL commands in an event trigger.
 *
 * This function calls the function pg_event_trigger_ddl_commands(), which is
 * part of the event trigger API, and retrieves the DDL commands executed in
 * relation to the event trigger. It is only valid to call this function from
 * within an event trigger.
 */
List *
event_trigger_ddl_commands(void)
{
	ReturnSetInfo rsinfo;
	FunctionCallInfoData fcinfo;
	TupleTableSlot *slot;
	EState	   *estate = CreateExecutorState();
	List	   *objects = NIL;

	InitFunctionCallInfoData(fcinfo, &ddl_commands_fmgrinfo, 1, InvalidOid, NULL, NULL);
	MemSet(&rsinfo, 0, sizeof(rsinfo));
	rsinfo.type = T_ReturnSetInfo;
	rsinfo.allowedModes = SFRM_Materialize;
	rsinfo.econtext = CreateExprContext(estate);
	fcinfo.resultinfo = (fmNodePtr) &rsinfo;

	FunctionCallInvoke(&fcinfo);

	slot = MakeSingleTupleTableSlot(rsinfo.setDesc);

	while (tuplestore_gettupleslot(rsinfo.setResult, true, false, slot))
	{
		HeapTuple	tuple = ExecFetchSlotTuple(slot);
		CollectedCommand *cmd;
		Datum		values[DDL_INFO_NATTS];
		bool		nulls[DDL_INFO_NATTS];

		heap_deform_tuple(tuple, rsinfo.setDesc, values, nulls);

		if (rsinfo.setDesc->natts > 8 && !nulls[8])
		{
			cmd = (CollectedCommand *) DatumGetPointer(values[8]);
			objects = lappend(objects, cmd);
		}
	}

	FreeExprContext(rsinfo.econtext, false);
	FreeExecutorState(estate);
	ExecDropSingleTupleTableSlot(slot);

	return objects;
}

/* Given a TEXT[] of addrnames return a list of char *
 *
 * similar to textarray_to_strvaluelist */
static List *
extract_addrnames(ArrayType *arr)
{
	Datum	   *elems;
	bool	   *nulls;
	int			nelems;
	List	   *list = NIL;
	int			i;

	deconstruct_array(arr, TEXTOID, -1, false, 'i',
					  &elems, &nulls, &nelems);

	for (i = 0; i < nelems; i++)
	{
		if (nulls[i])
			elog(ERROR, "unexpected null in name list");
		list = lappend(list, TextDatumGetCString(elems[i]));
	}

	return list;
}

List *
event_trigger_dropped_objects(void)
{
	ReturnSetInfo rsinfo;
	FunctionCallInfoData fcinfo;
	TupleTableSlot *slot;
	EState	   *estate = CreateExecutorState();
	List	   *objects = NIL;

	InitFunctionCallInfoData(fcinfo, &dropped_objects_fmgrinfo, 0, InvalidOid, NULL, NULL);
	MemSet(&rsinfo, 0, sizeof(rsinfo));
	rsinfo.type = T_ReturnSetInfo;
	rsinfo.allowedModes = SFRM_Materialize;
	rsinfo.econtext = CreateExprContext(estate);
	fcinfo.resultinfo = (fmNodePtr) &rsinfo;

	FunctionCallInvoke(&fcinfo);

	slot = MakeSingleTupleTableSlot(rsinfo.setDesc);

	while (tuplestore_gettupleslot(rsinfo.setResult, true, false, slot))
	{
		HeapTuple	tuple = ExecFetchSlotTuple(slot);
		EventTriggerDroppedObject *obj = palloc(sizeof(EventTriggerDroppedObject));
		Datum		values[DROPPED_OBJECTS_NATTS];
		bool		nulls[DROPPED_OBJECTS_NATTS];

		heap_deform_tuple(tuple, rsinfo.setDesc, values, nulls);

		obj->classid = DatumGetObjectId(values[0]);
		obj->objectid = DatumGetObjectId(values[1]);

		if (!nulls[6])
			obj->objtype = TextDatumGetCString(values[6]);
		else
			obj->objtype = NULL;

		if (!nulls[7])
			obj->schema = TextDatumGetCString(values[7]);
		else
			obj->schema = NULL;

		if (!nulls[8])
			obj->objname = TextDatumGetCString(values[8]);
		else
			obj->objname = NULL;

		obj->addrnames = extract_addrnames(DatumGetArrayTypeP(values[10]));

		objects = lappend(objects, obj);
	}

	FreeExprContext(rsinfo.econtext, false);
	FreeExecutorState(estate);
	ExecDropSingleTupleTableSlot(slot);

	return objects;
}

void
_event_trigger_init(void)
{
	fmgr_info(fmgr_internal_function("pg_event_trigger_ddl_commands"),
			  &ddl_commands_fmgrinfo);
	fmgr_info(fmgr_internal_function("pg_event_trigger_dropped_objects"),
			  &dropped_objects_fmgrinfo);
}

void
_event_trigger_fini(void)
{
	/* Nothing to do */
}
