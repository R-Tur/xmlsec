/** 
 * XMLSec library
 *
 * Buffered
 *
 * See Copyright for the status of this software.
 * 
 * Author: Aleksey Sanin <aleksey@aleksey.com>
 */
#include "globals.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <xmlsec/xmlsec.h>
#include <xmlsec/keys.h>
#include <xmlsec/transforms.h>
#include <xmlsec/transformsInternal.h>
#include <xmlsec/buffered.h>
#include <xmlsec/errors.h>

/****************************************************************************
 *
 *   BinTransform methods to be used in the Id structure
 *
 ****************************************************************************/
/**
 * xmlSecBufferedTransformRead:
 * @transform: the pointer to a buffered transform.
 * @buf: the output buffer.
 * @size: the output buffer size.
 *
 * Reads the all data from previous transform and returns 
 * to the caller.
 *
 * Returns the number of bytes in the buffer or negative value
 * if an error occurs.
 */
int  	
xmlSecBufferedTransformRead(xmlSecBinTransformPtr transform, 
			  unsigned char *buf, size_t size) {
    xmlSecBufferedTransformPtr buffered;
    size_t res = 0;
    int ret;

    xmlSecAssert2(transform != NULL, -1);
        
    if(!xmlSecBinTransformCheckSubType(transform, xmlSecBinTransformSubTypeBuffered)) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    XMLSEC_ERRORS_R_INVALID_TRANSFORM,
		    "xmlSecBinTransformSubTypeBuffered");    
	return(-1);
    }
    buffered = (xmlSecBufferedTransformPtr)transform;
    
    if((buf == NULL) || (size == 0)) {
	return(0);
    }

    if((buffered->status != xmlSecTransformStatusNone) || (buffered->prev == NULL)) {
	/* nothing to read (already called final or there are no previous transform */ 
	return(0);
    }

    if(buffered->buffer == NULL) {
	/*
	 * create the buffer, read everything from previous transform
	 * and call process method
	 */
	buffered->buffer = xmlBufferCreate();
	if(buffered->buffer == NULL) {
    	    xmlSecError(XMLSEC_ERRORS_HERE,
			XMLSEC_ERRORS_R_XML_FAILED,
			"xmlBufferCreate");
	    return(-1);
	}
	do {
	    ret = xmlSecBinTransformRead((xmlSecTransformPtr)buffered->prev, buf, size);
	    if(ret < 0) {
		xmlSecError(XMLSEC_ERRORS_HERE,
			    XMLSEC_ERRORS_R_XMLSEC_FAILED,
			    "xmlSecBinTransformRead");
		return(-1);
	    } else if(ret > 0) {
		xmlBufferAdd(buffered->buffer, buf, ret);
	    }
	} while(ret > 0);
	
	ret = xmlSecBufferedProcess(transform, buffered->buffer);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
	    		XMLSEC_ERRORS_R_XMLSEC_FAILED,
			"xmlSecBufferedProcess");
	    return(-1);
	}
    }
    
    res = xmlBufferLength(buffered->buffer);
    if(res <= size) {
	memcpy(buf, xmlBufferContent(buffered->buffer), res);
        buffered->status = xmlSecTransformStatusOk;  /* we are done */
	xmlBufferEmpty(buffered->buffer);
	xmlBufferFree(buffered->buffer);
	buffered->buffer = NULL;
    } else {
	res = size;
	memcpy(buf, xmlBufferContent(buffered->buffer), res);
	memset((void*)xmlBufferContent(buffered->buffer), 0, res);
	xmlBufferShrink(buffered->buffer, res);
    }
    
    return(res);
}

/**
 * xmlSecBufferedTransformWrite:
 * @transform: the poiter to a buffered transform.
 * @buf: the input data buffer.
 * @size: the input data size.
 *
 * Adds the data to the internal buffer.
 * 
 * Returns 0 if success or a negative value otherwise.
 */
int  	
xmlSecBufferedTransformWrite(xmlSecBinTransformPtr transform, 
                          const unsigned char *buf, size_t size) {
    xmlSecBufferedTransformPtr buffered;

    xmlSecAssert2(transform != NULL, -1);
        
    if(!xmlSecBinTransformCheckSubType(transform, xmlSecBinTransformSubTypeBuffered)) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    XMLSEC_ERRORS_R_INVALID_TRANSFORM,
		    "xmlSecBinTransformSubTypeBuffered");
	return(-1);
    }
    buffered = (xmlSecBufferedTransformPtr)transform;
    
    if((buf == NULL) || (size == 0)) {
	return(0);
    }

    if((buffered->status != xmlSecTransformStatusNone) || (buffered->next == NULL)) {
	/* nothing to read (already called final or there are no next transform */ 
	return(0);
    }

    if(buffered->buffer == NULL) {
	buffered->buffer = xmlBufferCreate();
	if(buffered->buffer == NULL) {
    	    xmlSecError(XMLSEC_ERRORS_HERE,
	    		XMLSEC_ERRORS_R_XML_FAILED,
			"xmlBufferCreate");
	    return(-1);
	}
    }
    xmlBufferAdd(buffered->buffer, buf, size);
    return(0);
}

/**
 * xmlSecBufferedTransformFlush:
 * @transform: the pointer to a buffered transform.
 * 
 * Writes internal data to previous transform.
 * 
 * Returns 0 if success or negative value otherwise.
 */
int
xmlSecBufferedTransformFlush(xmlSecBinTransformPtr transform) {
    xmlSecBufferedTransformPtr buffered;
    int ret;

    xmlSecAssert2(transform != NULL, -1);    

    if(!xmlSecBinTransformCheckSubType(transform, xmlSecBinTransformSubTypeBuffered)) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    XMLSEC_ERRORS_R_INVALID_TRANSFORM,
		    "xmlSecBinTransformSubTypeBuffered");
	return(-1);
    }
    buffered = (xmlSecBufferedTransformPtr)transform;
    
    if((buffered->status != xmlSecTransformStatusNone) || 
       (buffered->next == NULL) || (buffered->buffer == NULL)) {
	/* nothing to read (already called final or there are no next transform */ 
	return(0);
    }

    ret = xmlSecBufferedProcess(transform, buffered->buffer);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    "xmlSecBufferedProcess");
	return(-1);
    }
    
    ret = xmlSecBinTransformWrite((xmlSecTransformPtr)buffered->next, 
				  xmlBufferContent(buffered->buffer), 
				  xmlBufferLength(buffered->buffer));
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    "xmlSecBinTransformWrite");
	return(-1);
    }	  

    buffered->status = xmlSecTransformStatusOk;  /* we are done */
    xmlBufferEmpty(buffered->buffer);
    xmlBufferFree(buffered->buffer);
    buffered->buffer = NULL;

    /* do not forget to flush next transform */
    ret = xmlSecBinTransformFlush((xmlSecTransformPtr)buffered->next);
    if(ret < 0){
	xmlSecError(XMLSEC_ERRORS_HERE,
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    "xmlSecBinTransformFlush");
	return(-1);
    }	  
    return(0);
}

/** 
 * xmlSecBufferedDestroy
 * @buffered: the pointer to a buffered transform.
 *
 * Destroys the buffered transform.
 */
void 	
xmlSecBufferedDestroy(xmlSecBufferedTransformPtr buffered) {
    xmlSecAssert(buffered != NULL);

    if(buffered->buffer != NULL) {
	xmlBufferEmpty(buffered->buffer);
	xmlBufferFree(buffered->buffer);
    }
}

/** 
 * xmlSecBufferedProcess:
 * @transform: the pointer to a buffered transform.
 * @buffer: the buffered transform result,
 *
 * Executes buffered transform.
 *
 * Returns number of bytes processed or a negative value
 * if an error occurs.
 */
int 	
xmlSecBufferedProcess(xmlSecBinTransformPtr transform, xmlBufferPtr buffer) {
    xmlSecBufferedTransformPtr buffered;

    xmlSecAssert2(transform != NULL, -1);    
    xmlSecAssert2(buffer != NULL, -1);
    
    if(!xmlSecBinTransformCheckSubType(transform, xmlSecBinTransformSubTypeBuffered)) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    XMLSEC_ERRORS_R_INVALID_TRANSFORM,
		    "xmlSecBinTransformSubTypeBuffered");
	return(-1);
    }

    buffered = (xmlSecBufferedTransformPtr)transform;
    if(buffered->id->bufferedProcess != NULL) {
	return(buffered->id->bufferedProcess(buffered, buffer));
    }
    return(0);
}
