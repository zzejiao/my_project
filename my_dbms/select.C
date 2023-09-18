#include "catalog.h"
#include "query.h"


// forward declaration
const Status ScanSelect(const string & result, 
			const int projCnt, 
			const AttrDesc projNames[],
			const AttrDesc *attrDesc, 
			const Operator op, 
			const char *filter,
			const int reclen);

/*
 * Selects records from the specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Select(const string & result, 
		       const int projCnt, 
		       const attrInfo projNames[],
		       const attrInfo *attr, 
		       const Operator op, 
		       const char *attrValue)
{
   // Qu_Select sets up things and then calls ScanSelect to do the actual work
    cout << "Doing QU_Select" << endl;

	Status status;

	// go through the projection list and look up each in the
	// attr cat to get an AttrDesc structure
	AttrDesc attrDescArray[projCnt];
	for (int i = 0; i < projCnt; i++) {
		Status status = attrCat->getInfo(projNames[i].relName,
										 projNames[i].attrName,
										 attrDescArray[i]);
		if (status != OK) return status;
	}

	// get AttrDesc structure for the attribute to select from
	AttrDesc attrDesc;
	// get output record length from attrdesc structures
	int reclen = 0;
	for (int i = 0; i < projCnt; i++) {
		reclen += attrDescArray[i].attrLen;
	}

	// attr is NULL, an unconditional scan of the input table should be performed
	if (attr == NULL) {
		return ScanSelect(result, projCnt, attrDescArray, NULL, op, attrValue, reclen);
	}
	
	status = attrCat->getInfo(attr->relName, attr->attrName, attrDesc);
	if (status != OK) return status;

	return ScanSelect(result, projCnt, attrDescArray, &attrDesc, op, attrValue, reclen);
}

/*
 * Selects records from the specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */
const Status ScanSelect(const string & result, 
#include "stdio.h"
#include "stdlib.h"
			const int projCnt, 
			const AttrDesc projNames[],
			const AttrDesc *attrDesc, 
			const Operator op, 
			const char *filter,
			const int reclen)
{
    cout << "Doing HeapFileScan Selection using ScanSelect()" << endl;

	Status status;

	// open the result table
	InsertFileScan resultRel(result, status);
	if (status != OK) return status;

	// define the output record
	char outputData[reclen];
	Record outputRec;
	outputRec.data = (void *) outputData;
	outputRec.length = reclen;

	// construct a new scan
	HeapFileScan scan(string(projNames[0].relName), status);
	if (status != OK) return status;

	// if attrDesc is NULL, start an unconditional scan
	if (attrDesc == NULL) {
		status = scan.startScan(0, 0, STRING, NULL, EQ);
		if (status != OK) return status;
	} else {
		// start scan on the table
		scan = HeapFileScan(string(attrDesc->relName), status);
		if (status != OK) return status;

		// set startScan using the proper data type
		switch ((Datatype) (attrDesc->attrType)) {
			case STRING:
				status = scan.startScan(attrDesc->attrOffset, 
									attrDesc->attrLen, 
									(Datatype) (attrDesc->attrType), 
									filter, 
									op);
				break;
			
			case INTEGER: {
				int filterValue = atoi(filter);
				status = scan.startScan(attrDesc->attrOffset, 
									attrDesc->attrLen, 
									(Datatype) (attrDesc->attrType), 
									(char *) &filterValue, 
									op);
				break;
			}

			case FLOAT: {
				float filterValue = atof(filter);
				status = scan.startScan(attrDesc->attrOffset, 
									attrDesc->attrLen, 
									(Datatype) (attrDesc->attrType), 
									(char *) &filterValue, 
									op);
				break;
			}
		}
		
		if (status != OK) return status;
	}

	// scan the relation table
	RID matchedRID;
	while (scan.scanNext(matchedRID) == OK) {
		Record matchedRec;
		status = scan.getRecord(matchedRec);
		ASSERT(status == OK);

		// we have a match, copy data into the output record
		int outputOffset = 0;
		for (int i = 0; i < projCnt; i++) {
			// copy the data out of the input
			memcpy(outputData + outputOffset,
					(char *)matchedRec.data + projNames[i].attrOffset,
					projNames[i].attrLen);	
			
			// update output offset
			outputOffset += projNames[i].attrLen;
		}

		// add the new record to the output relation
		RID outRID;
		status = resultRel.insertRecord(outputRec, outRID);
		ASSERT(status == OK);
	}
	return OK;
}
