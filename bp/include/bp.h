/*
 	bp.h:	definitions supporting applications built on the
		implementation of the Bundle Protocol in the ION
		(Interplanetary Overlay Network) stack.

	Author: Scott Burleigh, JPL

	Modification History:
	Date  Who What

	Copyright (c) 2007, California Institute of Technology.
	ALL RIGHTS RESERVED.  U.S. Government Sponsorship
	acknowledged.
 									*/
#ifndef _BP_H_
#define _BP_H_

#include "platform.h"
#include "psm.h"
#include "sdr.h"
#include "zco.h"
#include "lyst.h"
#include "smlist.h"
#include "ion.h"

#ifdef __cplusplus
extern "C" {
#endif

/*	bp_send and bp_receive timeout values				*/
#define	BP_POLL			(0)	/*	Return immediately.	*/
#define	BP_NONBLOCKING		(0)	/*	Return immediately.	*/
#define BP_BLOCKING		(-1)	/*	Wait forever.		*/

/*	bp_send priority values						*/
#define	BP_BULK_PRIORITY	(0)	/*	Slower.			*/
#define	BP_STD_PRIORITY		(1)	/*	Faster.			*/
#define	BP_EXPEDITED_PRIORITY	(2)	/*	Precedes others.	*/

typedef enum
{
	NoCustodyRequested = 0,
	SourceCustodyOptional,
	SourceCustodyRequired
} BpCustodySwitch;

/*	Status report request flag values				*/
#define BP_RECEIVED_RPT		(1)	/*	00000001		*/
#define BP_CUSTODY_RPT		(2)	/*	00000010		*/
#define BP_FORWARDED_RPT	(4)	/*	00000100		*/
#define BP_DELIVERED_RPT	(8)	/*	00001000		*/
#define BP_DELETED_RPT		(16)	/*	00010000		*/

typedef struct
{
	unsigned long	flowLabel;	/*	Optional.		*/
	unsigned char	flags;		/*	See below.		*/
	unsigned char	ordinal;	/*	0 to 254 (most urgent).	*/
} BpExtendedCOS;

/*	Extended class-of-service flags.				*/
#define	BP_MINIMUM_LATENCY	(1)	/*	Forward on all routes.	*/
#define	BP_BEST_EFFORT		(2)	/*	Unreliable CL is okay.	*/
#define	BP_FLOW_LABEL_PRESENT	(4)	/*	Ignore flow label if 0.	*/

typedef struct bpsap_st		*BpSAP;

typedef struct
{
	unsigned long	seconds;
	unsigned long	count;
} BpTimestamp;

typedef enum
{
	BpPayloadPresent = 1,
	BpReceptionTimedOut,
	BpReceptionInterrupted
} BpIndResult;

typedef struct
{
	BpIndResult	result;
	char		*bundleSourceEid;
	BpTimestamp	bundleCreationTime;
	int		ackRequested;	/*	(By app.)  Boolean.	*/
	int		adminRecord;	/*	Boolean: 0 = non-admin.	*/
	Object		adu;		/*	Zero-copy object ref.	*/
} BpDelivery;

extern int		bp_attach();
			/* 	Note that all ION libraries and
			 * 	applications draw memory dynamically,
			 * 	as needed, from a shared pool of ION
			 * 	working memory.  The size of the pool
			 * 	is established when it is first
			 * 	accessed by one of the ION
			 * 	administrative programs, either
			 * 	bpadmin or ltpadmin.
			 *
			 *	Returns 0 on success, -1 on any error.	*/

extern int		bp_agent_is_started();
			/*	Returns 1 if the local BP agent has
			 *	been started and not yet stopped, 0
			 *	otherwise.				*/

extern Sdr		bp_get_sdr();
			/*	Returns the SDR used for BP, to enable
			 *	creation and interrogation of bundle
			 *	payloads (application data units).	*/

extern void		bp_detach();
			/*	Terminates access to local BP agent.	*/

extern int		bp_open(	char *eid,
					BpSAP *ionsapPtr);
			/*	Arguments are:
		 	 *  		name of the endpoint 
			 *		pointer to variable in which
			 *			address of BP service
			 *			access point will be
			 *			returned
			 *
			 * 	Initiates ability to take delivery
			 *	of bundles destined for the indicated
			 *	endpoint and to send bundles whose
			 *	source is the indicated endpoint.
			 *
			 *	Returns 0 on success, -1 on any error.	*/

extern int		bp_send(	BpSAP sap,
					int mode,
					char *destEid,
					char *reportToEid,
					int lifespan,
					int classOfService,
					BpCustodySwitch custodySwitch,
					unsigned char srrFlags,
					int ackRequested,
					BpExtendedCOS *extendedCOS,
					Object adu,
					Object *newBundle);
			/*	mode must be either BP_BLOCKING or
			 *	BP_NONBLOCKING.  bp_send does not
			 *	support timeout intervals.
			 *
			 *	Class of service is simply priority
			 *	for now.  If class-of-service flags
			 *	are defined in a future version of
			 *	Bundle Protocol, those flags would
			 *	be OR'd with priority.
			 *
			 *	Extended class of service, if not
			 *	NULL, is used to populate the extended
			 *	class of service block.  Flag values
			 *	are OR'd together.  If this argument
			 *	is NULL, the default flags and ordinal
			 *	values are 0 and there is no flow
			 *	label.
			 *
			 *	adu must be a "zero-copy object"
			 *	reference as returned by zco_create().
			 *
			 *	Returns 1 on success, 0 on transient
			 *	failure, -1 on any other (i.e., system
			 *	or application; permanent) error.  If
			 *	1 is returned, then the ADU has been
			 *	accepted and queued for transmission
			 *	in a bundle.  If 0 is returned, then
			 *	either there is not currently enough
			 *	space for acceptance and queuing of
			 *	this ADU or else the destination or
			 *	report-to endpoint ID is malformed.
			 *	In the former case (which is possible
			 *	only when the "blocking" flag passed
			 *	to bp_send is zero), errno has been
			 *	set to EWOULDBLOCK; the application
			 *	may abandon this transmission attempt
			 *	or may instead wait briefly and then
			 *	try again.				*/

extern int		bp_track(	Object bundleObj,
					Object trackingElt);
			/*	Adds trackingElt to the list of
			 *	"tracking" reference in the bundle.
			 *	trackingElt must be the address of
			 *	an SDR list element -- whose data is
			 *	the address of this same bundle --
			 *	within some list of bundles that is
			 *	privately managed by the application.
			 *	Upon destruction of the bundle this
			 *	list element will automatically be
			 *	deleted, thus removing the bundle from
			 *	the application's privately managed
			 *	list of bundles.  This enables the
			 *	application to keep track of bundles
			 *	that it is operating on without risk
			 *	of inadvertently de-referencing the
			 *	address of a nonexistent bundle.	*/

extern void		bp_untrack(	Object bundleObj,
					Object trackingElt);
			/*	Removes trackingElt from the bundle's
			 *	list of "tracking" references, if it
			 *	is in that list.  Does not delete
			 *	trackingElt itself.			*/

extern int		bp_cancel(	Object bundleObj);
			/*	Cancels transmission of this bundle.	*/

extern int		bp_receive(	BpSAP sap,
					BpDelivery *dlvBuffer,
					int timeoutSeconds);
			/*	The "result" field of the dlvBuffer
			 *	structure will be used to indicate the
			 *	outcome of the data reception activity.
			 *
			 *	If at least one bundle destined for
			 *	the endpoint for which this SAP is
			 *	opened has not yet been delivered
			 *	to the SAP, then the payload of the
			 *	oldest such bundle will be returned in
			 *	dlvBuffer->adu and dlvBuffer->result
			 *	will be set to BpPayloadPresent.  If
			 *	there is no such bundle, bp_receive
			 *	blocks for up to timeoutSeconds while
			 *	waiting for one to arrive.
			 *
			 *	If timeoutSeconds is BP_POLL and no
			 *	bundle is awaiting delivery, or if
			 *	timeoutSeconds is greater than zero but
			 *	no bundle arrives before timeoutSeconds
			 *	have elapsed, then dlvBuffer->result
			 *	will be set to BpReceptionTimedOut.
			 *
			 *	dlvBuffer->result will be set to
			 *	BpReceptionInterrupted in the event
			 *	that the calling process received and
			 *	handled some signal other than SIGALRM
			 *	while waiting for a bundle.
			 *
			 *	The application data unit delivered
			 *	in the data delivery structure, if
			 *	any, will be a "zero-copy object"
			 *	reference; use the zco_receive_XXX
			 *	functions to read the content of the
			 *	application data unit.
			 *
			 *	Be sure to call bp_release_delivery()
			 *	after every successful invocation of
			 *	bp_receive().
			 *
			 *	Returns 0 on success, -1 on any error.	*/

extern void		bp_interrupt(BpSAP);
			/*	Interrupts an bp_receive invocation
			 *	that is currently blocked.  Designed
			 *	to be called from a signal handler;
			 *	for this purpose, the BpSAP may need
			 *	to be retained in a static variable.	*/

extern void		bp_release_delivery(BpDelivery *dlvBuffer,
					int releaseAdu);
			/*	Releases resources allocated to the
			 *	indicated delivery.  releaseAdu
			 *	is a Boolean parameter: if non-zero,
			 *	the ADU ZCO reference in dlvBuffer
			 *	(if any) is destroyed, causing the
			 *	ZCO itself to be destroyed if no
			 *	other references to it remain.		*/

extern void		bp_close(BpSAP sap);
			/*	Terminates access to the bundles
			 *	enqueued for the endpoint cited by
			 *	the indicated service access point.	*/

#ifdef __cplusplus
}
#endif

#endif	/* _BP_H */
