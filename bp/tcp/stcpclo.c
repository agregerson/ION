/*
	stcpclo.c:	BP TCP-based convergence-layer output
			daemon.  Note that this convergence-layer
			output daemon is a "dedicated" CLO daemon
			suitable only for a limited number of paths,
			because it manages just a single TCP
			connection.

			Promiscuous CLO daemons need to be based on
			UDP, Dgr, etc.

	Author: Scott Burleigh, JPL

	Copyright (c) 2004, California Institute of Technology.
	ALL RIGHTS RESERVED.  U.S. Government Sponsorship
	acknowledged.
	
									*/
#include "tcpcla.h"

static sm_SemId		stcpcloSemaphore(sm_SemId *semid)
{
	static sm_SemId	semaphore = -1;

	if (semid)
	{
		semaphore = *semid;
		sm_TaskVarAdd(&semaphore);
	}

	return semaphore;
}

static void	shutDownClo()	/*	Commands CLO termination.	*/
{
	sm_SemEnd(stcpcloSemaphore(NULL));
}

/*	*	*	Keepalive thread functions	*	*	*/

typedef struct
{
	int		*cloRunning;
	pthread_mutex_t	*mutex;
	struct sockaddr	*socketName;
	int		*ductSocket;
} KeepaliveThreadParms;

static void	*sendKeepalives(void *parm)
{
	KeepaliveThreadParms	*parms = (KeepaliveThreadParms *) parm;
	int			count = KEEPALIVE_PERIOD;
	int			bytesSent;

	iblock(SIGTERM);
	while (*(parms->cloRunning))
	{
		snooze(1);
		count++;
		if (count < KEEPALIVE_PERIOD)
		{
			continue;
		}

		/*	Time to send a keepalive.  Note that the
		 *	interval between keepalive attempts will be
		 *	KEEPALIVE_PERIOD plus (if the remote induct
		 *	is not reachable) the length of time taken
		 *	by TCP to determine that the connection
		 *	attempt will not succeed (e.g., 3 seconds).	*/

		count = 0;
		pthread_mutex_lock(parms->mutex);
		bytesSent = sendBundleByTCP(parms->socketName,
				parms->ductSocket, 0, 0, NULL);
		pthread_mutex_unlock(parms->mutex);
		if (bytesSent < 0)
		{
			shutDownClo();
			break;
		}
	}

	return NULL;
}

/*	*	*	Main thread functions	*	*	*	*/

#if defined (VXWORKS) || defined (RTEMS)
int	stcpclo(int a1, int a2, int a3, int a4, int a5,
		int a6, int a7, int a8, int a9, int a10)
{
	char	*ductName = (char *) a1;
#else
int	main(int argc, char *argv[])
{
	char	*ductName = (argc > 1 ? argv[1] : NULL);
#endif
	unsigned char		*buffer;
	VOutduct		*vduct;
	PsmAddress		vductElt;
	Sdr			sdr;
	Outduct			duct;
	ClProtocol		protocol;
	Outflow			outflows[3];
	int			i;
	char			*hostName;
	unsigned short		portNbr;
	unsigned int		hostNbr;
	struct sockaddr		socketName;
	struct sockaddr_in	*inetName;
	int			running = 1;
	pthread_mutex_t		mutex;
	KeepaliveThreadParms	parms;
	pthread_t		keepaliveThread;
	Object			bundleZco;
	BpExtendedCOS		extendedCOS;
	char			destDuctName[MAX_CL_DUCT_NAME_LEN + 1];
	unsigned int		bundleLength;
	int			ductSocket = -1;
	int			bytesSent;

	if (ductName == NULL)
	{
		PUTS("Usage: stcpclo <remote host name>[:<port number>]");
		return 0;
	}

	if (bpAttach() < 0)
	{
		putErrmsg("stcpclo can't attach to BP.", NULL);
		return -1;
	}

	buffer = MTAKE(TCPCLA_BUFSZ);
	if (buffer == NULL)
	{
		putErrmsg("No memory for TCP buffer in stcpclo.", NULL);
		return -1;
	}

	findOutduct("stcp", ductName, &vduct, &vductElt);
	if (vductElt == 0)
	{
		putErrmsg("No such stcp duct.", ductName);
		MRELEASE(buffer);
		return -1;
	}

	if (vduct->cloPid > 0 && vduct->cloPid != sm_TaskIdSelf())
	{
		putErrmsg("CLO task is already started for this duct.",
				itoa(vduct->cloPid));
		MRELEASE(buffer);
		return -1;
	}

	/*	All command-line arguments are now validated.		*/

	sdr = getIonsdr();
	sdr_read(sdr, (char *) &duct, sdr_list_data(sdr, vduct->outductElt),
			sizeof(Outduct));
	sdr_read(sdr, (char *) &protocol, duct.protocol, sizeof(ClProtocol));
	if (protocol.nominalRate <= 0)
	{
		vduct->xmitThrottle.nominalRate = DEFAULT_TCP_RATE;
	}
	else
	{
		vduct->xmitThrottle.nominalRate = protocol.nominalRate;
	}

	memset((char *) outflows, 0, sizeof outflows);
	outflows[0].outboundBundles = duct.bulkQueue;
	outflows[1].outboundBundles = duct.stdQueue;
	outflows[2].outboundBundles = duct.urgentQueue;
	for (i = 0; i < 3; i++)
	{
		outflows[i].svcFactor = 1 << i;
	}

	hostName = ductName;
	parseSocketSpec(ductName, &portNbr, &hostNbr);
	if (portNbr == 0)
	{
		portNbr = BpTcpDefaultPortNbr;
	}

	portNbr = htons(portNbr);
	if (hostNbr == 0)
	{
		putErrmsg("Can't get IP address for host.", hostName);
		MRELEASE(buffer);
		return -1;
	}

	hostNbr = htonl(hostNbr);
	memset((char *) &socketName, 0, sizeof socketName);
	inetName = (struct sockaddr_in *) &socketName;
	inetName->sin_family = AF_INET;
	inetName->sin_port = portNbr;
	memcpy((char *) &(inetName->sin_addr.s_addr), (char *) &hostNbr, 4);

	/*	Set up signal handling.  SIGTERM is shutdown signal.	*/

	oK(stcpcloSemaphore(&(vduct->semaphore)));
	isignal(SIGTERM, shutDownClo);
	isignal(SIGPIPE, handleConnectionLoss);

	/*	Start the keepalive thread for the eventual connection.	*/

	parms.cloRunning = &running;
	pthread_mutex_init(&mutex, NULL);
	parms.mutex = &mutex;
	parms.socketName = &socketName;
	parms.ductSocket = &ductSocket;
	if (pthread_create(&keepaliveThread, NULL, sendKeepalives, &parms))
	{
		putSysErrmsg("stcpclo can't create keepalive thread", NULL);
		MRELEASE(buffer);
		pthread_mutex_destroy(&mutex);
		return 1;
	}

	/*	Can now begin transmitting to remote duct.		*/

	writeMemo("[i] stcpclo is running.");
	while (!(sm_SemEnded(stcpcloSemaphore(NULL))))
	{
		if (bpDequeue(vduct, outflows, &bundleZco, &extendedCOS,
				destDuctName) < 0)
		{
			sm_SemEnd(stcpcloSemaphore(NULL));/*	Stop.	*/
			continue;
		}

		if (bundleZco == 0)	/*	Interrupted.		*/
		{
			continue;
		}

		bundleLength = zco_length(sdr, bundleZco);
		pthread_mutex_lock(&mutex);
		bytesSent = sendBundleByTCP(&socketName, &ductSocket,
				bundleLength, bundleZco, buffer);
		pthread_mutex_unlock(&mutex);
		if (bytesSent < bundleLength)
		{
			sm_SemEnd(stcpcloSemaphore(NULL));/*	Stop.	*/
			continue;
		}

		/*	Make sure other tasks have a chance to run.	*/

		sm_TaskYield();
	}

	running = 0;			/*	Terminate keepalive.	*/
	pthread_join(keepaliveThread, NULL);
	if (ductSocket != -1)
	{
		close(ductSocket);
	}

	pthread_mutex_destroy(&mutex);
	writeErrmsgMemos();
	writeMemo("[i] stcpclo duct has ended.");
	MRELEASE(buffer);
	ionDetach();
	return 0;
}
