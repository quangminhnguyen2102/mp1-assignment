/**********************************
 * FILE NAME: Application.cpp
 *
 * DESCRIPTION: Application layer class function definitions
 **********************************/

#include "Application.h"

void handler(int sig) {
	void *array[10];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 10);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);
	exit(1);
}

/**********************************
 * FUNCTION NAME: main
 *
 * DESCRIPTION: main function. Start from here
 **********************************/
int main(int argc, char *argv[]) {
	//signal(SIGSEGV, handler);
	if ( argc != ARGS_COUNT ) {
		cout<<"Configuration (i.e., *.conf) file File Required"<<endl;
		return FAILURE;
	}

	// Create a new application object
	Application *app = new Application(argv[1]);
	// Call the run function
	app->run();
	// When done delete the application object
	delete(app);

	return SUCCESS;
}

/**
 * Constructor of the Application class
 */
Application::Application(char *infile) {
	int i;
	par = new Params();	//goi ham Parameter khoi tao
	srand (time(NULL));	//Ramdom thoi gian
	par->setparams(infile);	//goi function setparams truyen vao infile->tao ra gia tri 
					//	MAX_NNB, SINGLE_FAILURE, DROP_MSG, MSG_DROP_PROB,allNodesJoined=10;EN_GPSZ=10
	log = new Log(par);	//khoi tao log voi params  
	en = new EmulNet(par);	/khoi tao Emulet voi params
	mp1 = (MP1Node **) malloc(par->EN_GPSZ * sizeof(MP1Node *)); //cap phat o nho cho mp1 voi kich thuoc la 10

	/*
	 * Init all nodes
	 */
	for( i = 0; i < par->EN_GPSZ; i++ ) { //for i=0...10 lay ra dia chi IP: 1.0.0.0:0 APP...
		Member *memberNode = new Member;
		memberNode->inited = false;
		Address *addressOfMemberNode = new Address();
		Address joinaddr;
		joinaddr = getjoinaddr(); //ham lay ra dia chi
		addressOfMemberNode = (Address *) en->ENinit(addressOfMemberNode, par->PORTNUM);
		mp1[i] = new MP1Node(memberNode, par, en, log, addressOfMemberNode);
		log->LOG(&(mp1[i]->getMemberNode()->addr), "APP");
		delete addressOfMemberNode;
	}
}

/**
 * Destructor
 */
Application::~Application() {
	delete log;
	delete en;
	for ( int i = 0; i < par->EN_GPSZ; i++ ) {
		delete mp1[i];
	}
	free(mp1);
	delete par;
}

/**
 * FUNCTION NAME: run
 *
 * DESCRIPTION: Main driver function of the Application layer
 */
int Application::run()
{
	int i;
	int timeWhenAllNodesHaveJoined = 0;
	// boolean indicating if all nodes have joined
	bool allNodesJoined = false;
	srand(time(NULL));

	// As time runs along
	for( par->globaltime = 0; par->globaltime < TOTAL_RUNNING_TIME; ++par->globaltime ) { //Define TOTAL_RUNNING_TIME =700
		// Run the membership protocol
		// printf("STEPRATE run= %lf\n",par->STEP_RATE);
		mp1Run();
		// Fail some nodes
		fail();
	}

	// Clean up
	en->ENcleanup();

	for(i=0;i<=par->EN_GPSZ-1;i++) {
		 mp1[i]->finishUpThisNode();
	}

	return SUCCESS;
}

/**
 * FUNCTION NAME: mp1Run
 *
 * DESCRIPTION:	This function performs all the membership protocol functionalities
 */
void Application::mp1Run() {
	int i;
	// printf("EN_GPSZ= %d\n",par->EN_GPSZ);
	// For all the nodes in the system
	for( i = 0; i <= par->EN_GPSZ-1; i++) {     	//EN_GPSZ=10


		/*
		 * Receive messages from the network and queue them in the membership protocol queue
		 */
	// curerent time tang theo i ham run() goi.
		 // printf("STEPRATE= %lf; getcurrtime= %d\n",par->STEP_RATE,par->getcurrtime());
		 // printf("Kieu In= %d\n",(int)(par->STEP_RATE*i)  );
		 //getcurrtime =0 -> 699
		if( par->getcurrtime() > (int)(par->STEP_RATE*i) && !(mp1[i]->getMemberNode()->bFailed) ) 
		// if(timecurrent>0.25*i) && member->bfail=true)
		{ 
			// Receive messages from the network and queue them
			mp1[i]->recvLoop();
		}

	}

	// For all the nodes in the system
	for( i = par->EN_GPSZ - 1; i >= 0; i-- ) {

		/*
		 * Introduce nodes into the distributed system
		 */
		if( par->getcurrtime() == (int)(par->STEP_RATE*i) ) // khi currenttime =0,1,2; i=0,4,8
		{
		// printf("\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		// printf("getcurrtime= %d; STEPRATE =%d,; i= %d; (STEPRATE*i)= %d", par->getcurrtime(),(int)par->STEP_RATE,i,(int)par->STEP_RATE*i);
		// printf("\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		
			// introduce the ith node into the system at time STEPRATE*i
			mp1[i]->nodeStart(JOINADDR, par->PORTNUM);
			cout<<i<<"-th introduced node is assigned with the address: "<<mp1[i]->getMemberNode()->addr.getAddress() << endl;
			nodeCount += i;
		}

		/*
		 * Handle all the messages in your queue and send heartbeats
		 */
		else if( par->getcurrtime() > (int)(par->STEP_RATE*i) && !(mp1[i]->getMemberNode()->bFailed) ) {
			// handle messages and send heartbeats
			mp1[i]->nodeLoop();
			#ifdef DEBUGLOG
			if( (i == 0) && (par->globaltime % 500 == 0) ) {
				log->LOG(&mp1[i]->getMemberNode()->addr, "@@time=%d", par->getcurrtime());
			}
			#endif
		}

	}
}

/**
 * FUNCTION NAME: fail
 *
 * DESCRIPTION: This function controls the failure of nodes
 *
 * Note: this is used only by MP1
 */
void Application::fail() {
	int i, removed;

	// fail half the members at time t=400
	// printf("par->DROP_MSG= %d\n",par->DROP_MSG );
	if( par->DROP_MSG && par->getcurrtime() == 50 )	//par->DROP_MSG ???? 0 ; par->getcurrtime() == 50 thi par->dropmsg = 1;														// par->getcurrtime() == 300 thi par->dropmsg = 0;
	{
		par->dropmsg = 1;
	}

	if( par->SINGLE_FAILURE && par->getcurrtime() == 100 ) {
		removed = (rand() % par->EN_GPSZ);
		#ifdef DEBUGLOG
		log->LOG(&mp1[removed]->getMemberNode()->addr, "Node 1 failed at time=%d", par->getcurrtime());
		#endif
		mp1[removed]->getMemberNode()->bFailed = true;
	}
	else if( par->getcurrtime() == 100 ) {
		removed = rand() % par->EN_GPSZ/2;
		for ( i = removed; i < removed + par->EN_GPSZ/2; i++ ) {
			#ifdef DEBUGLOG
			log->LOG(&mp1[i]->getMemberNode()->addr, "Node failed at time = %d", par->getcurrtime());
			#endif
			mp1[i]->getMemberNode()->bFailed = true;
		}
	}

	if( par->DROP_MSG && par->getcurrtime() == 300) {
		par->dropmsg=0;
	}

}

/**
 * FUNCTION NAME: getjoinaddr
 *
 * DESCRIPTION: This function returns the address of the coordinator
 */
Address Application::getjoinaddr(void){
	//trace.funcEntry("Application::getjoinaddr");
    Address joinaddr;
    joinaddr.init();
    *(int *)(&(joinaddr.addr))=1;
    *(short *)(&(joinaddr.addr[4]))=0;
    //trace.funcExit("Application::getjoinaddr", SUCCESS);
    return joinaddr;
}
