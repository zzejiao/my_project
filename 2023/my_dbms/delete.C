#include "catalog.h"
#include "query.h"


/*
 * Deletes records from a specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Delete(const string & relation, 
		       const string & attrName, 
		       const Operator op,
		       const Datatype type, 
		       const char *attrValue)
{
	cout << "Doing QU_Delete" << endl;

	Status status;
	AttrDesc attrDesc;
	RID relID;
	const char *filter;

	HeapFileScan relScan(relation, status);
	if(status != OK){
		return status;
	}

	//if there's no predite specified, delete all tulples
	if(attrName.length() == 0){
		// setting scan start point at offset 0 with no filter applied
		status = relScan.startScan(0, 0, STRING, NULL, EQ);
		if (status != OK) {
			return status;
		}
		while(relScan.scanNext(relID) == OK){//assigning RID 
			status = relScan.deleteRecord();
			if (status != OK) return status;
		}

		return OK;
	}

	//if there's a filter for deleting
	//getting search filter information

	status = attrCat->getInfo(relation, attrName, attrDesc);
	if(status != OK ) return status;

	int tmpInt; 
	float tmpFloat;

	//manually converting to the right data type
	switch (type){
		case INTEGER:{
			tmpInt = atoi(attrValue);
			filter = (char*)&tmpInt;
			break;
			}
		case STRING:{
			filter = attrValue;
			break;
		}
		case FLOAT: {
			tmpFloat = atof(attrValue);
			filter = (char*)&tmpFloat;
			break;
		}

	}

	// now delete the specified tulples
	status = relScan.startScan(attrDesc.attrOffset, attrDesc.attrLen, type, filter, op);

		while(relScan.scanNext(relID) == OK){//assigning RID 
			status = relScan.deleteRecord();
			if (status != OK) return status;
		}

	return OK;

}


