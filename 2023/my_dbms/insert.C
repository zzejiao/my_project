#include "catalog.h"
#include "query.h"


/*
 * Inserts a record into the specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Insert(const string & relation, 
	const int attrCnt, 
	const attrInfo attrList[])
{
	cout << "Doing QU_Insert" << endl;
	// part 6
	Status status;

	// go through the attrList and look up each in the
	// attr cat to get an AttrDesc structure
	AttrDesc attrDescArray[attrCnt];
	for (int i = 0; i < attrCnt; i++) {
		Status status = attrCat->getInfo(attrList[i].relName,
										 attrList[i].attrName,
										 attrDescArray[i]);
		if (status != OK) return status;
	}
	
	// open the relation table to be inserted
	InsertFileScan insertedRel(relation, status);
	if (status != OK) return status;

	// compute the record length
	int reclen = 0;
	for (int i = 0; i < attrCnt; i++) {
		reclen += attrDescArray[i].attrLen;
	}

	// define the record to be inserted
	char insertData[reclen];
	Record insertRec;
	insertRec.data = (void *) insertData;
	insertRec.length = reclen;

	for (int i = 0; i < attrCnt; i++) {
		// cast attrValue to the proper type
		char * insertValue;
		switch (attrDescArray[i].attrType)
		{
			case STRING: {
				insertValue = (char *) attrList[i].attrValue;
				break;
			}
			
			case INTEGER: {
				int value = atoi((char *)attrList[i].attrValue);
				insertValue = (char *) &value;
				break;
			}

			case FLOAT: {
				float value = atof((char *)attrList[i].attrValue);
				insertValue = (char *) &value;
				break;
			}
		}

		// copy the data out of the input
		memcpy(insertData + attrDescArray[i].attrOffset,
				insertValue,
				attrDescArray[i].attrLen);
	}

	// insert the record
	RID insertRID;
	status = insertedRel.insertRecord(insertRec, insertRID);
	if (status != OK) return status;

	return OK;
}

