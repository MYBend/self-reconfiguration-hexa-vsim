#ifndef TpHexagonCode_H_
#define TpHexagonCode_H_

#include "robots/hexanodes/hexanodesSimulator.h"
#include "robots/hexanodes/hexanodesWorld.h"
#include "robots/hexanodes/hexanodesBlockCode.h"
#include "robots/hexanodes/hexanodesMotionEvents.h"
#include "robots/hexanodes/hexanodesMotionEngine.h"
#include "grid/lattice.h"
static const int BROADCAST_MSG_ID = 1001;
static const int FORECAST_MSG_ID = 1002;
static const int BACK_MSG_ID = 1003;
static const int GO_MSG_ID = 1004;
static const int COUNT_MSG_ID = 1005;
static const int SPEED = 10000;
static const Cell3DPosition NO_POSITION = Cell3DPosition(-1,-1,-1);

using namespace Hexanodes;

class TpHexagonCode : public HexanodesBlockCode {
private:
  
  int currentChildId = 0;
	int myDistance=0;
  int parent_id = 0;
	int round=0;

  bool isLeader=false;
  bool otherPossibility = true;
  bool shouldContinue = true;
	bool inTarget = false;

  Cell3DPosition previous_position; 
  Cell3DPosition initial = NO_POSITION;

  P2PNetworkInterface* parent= nullptr;

  vector<P2PNetworkInterface*> children;
  vector<P2PNetworkInterface*> waiting_from;
  vector<P2PNetworkInterface*> to_send;

  HexanodesWorld *wrl;
	HexanodesBlock *node = nullptr;

public :
  inline static int nMotions = 0;
	TpHexagonCode(HexanodesBlock *host);
	~TpHexagonCode() {};

  pair<bool,Cell3DPosition> isConnector() {
    int n = 0;
    while (n < HHLattice::MAX_NB_NEIGHBORS &&
            ( !lattice->isFree(node->position + (static_cast<HHLattice *>(lattice))->getNeighborRelativePos(
                    static_cast<HHLattice::Direction>(n)))
                    ||
           !target->isInTarget(node->position + (static_cast<HHLattice *>(lattice))->getNeighborRelativePos(
                   static_cast<HHLattice::Direction>(n))))) {
        n++;
    }
    return (n < HHLattice::MAX_NB_NEIGHBORS?
      make_pair(true,node->position + (static_cast<HHLattice *>(lattice))->getNeighborRelativePos(
              static_cast<HHLattice::Direction>(n)))
              :make_pair(false,Cell3DPosition(0,0,0)));
  }
   vector<P2PNetworkInterface*> removeElement(P2PNetworkInterface* elmt,vector<P2PNetworkInterface*> vec){
    int i = 0;
    int length = vec.size();
    while(i<length && vec[i] != elmt)
      i++;
    if(i != length)
      vec.erase(vec.begin() + i);
    return vec;

  }
  void ReceivedMessage(MessagePtr msg, P2PNetworkInterface* sender);
/**
  * This function is called on startup of the blockCode, it can be used to perform initial
  *  configuration of the host or this instance of the program.
  * @note this can be thought of as the main function of the module
  */
    void startup() override;

/**
  * @brief Message handler for the message 'msg'
  * @param _msg Pointer to the message received by the module, requires casting
  * @param sender Connector of the module that has received the message and that is connected to the sender
  */
   void myMsgFunc(std::shared_ptr<Message>_msg,P2PNetworkInterface *sender);

/**
  * @brief Provides the user with a pointer to the configuration file parser, which can be used to read additional user information from it.
  * @param config : pointer to the TiXmlDocument representing the configuration file, all information related to VisibleSim's core have already been parsed
  *
  * Called from BuildingBlock constructor, only once.
  */
    void parseUserElements(TiXmlDocument *config) override {};

/**
  * @brief Provides the user with a pointer to the configuration file parser, which can be used to read additional user information from each block config. Has to be overridden in the child class.
  * @param config : pointer to the TiXmlElement representing the block configuration file, all information related to concerned block have already been parsed
  *
  */
    void parseUserBlockElements(TiXmlElement *config) override;

/**
  * User-implemented debug function that gets called when a module is selected in the GUI
  */
    void onBlockSelected() override;

/**
  * @brief Callback function executed whenever the module finishes a motion
  */
    void onMotionEnd() override;


/**
  * User-implemented keyboard handler function that gets called when
  *  a key press event could not be caught by openglViewer
  * @param c key that was pressed (see openglViewer.cpp)
  * @param x location of the pointer on the x axis
  * @param y location of the pointer on the y axis
  * @note call is made from GlutContext::keyboardFunc (openglViewer.h)
  */
    void onUserKeyPressed(unsigned char c, int x, int y) override;
    void myBroadcastFunc(std::shared_ptr<Message>_msg,P2PNetworkInterface *sender);
    void myForecastFunc(std::shared_ptr<Message>_msg,P2PNetworkInterface *sender);
    void myGoFunc(std::shared_ptr<Message>_msg,P2PNetworkInterface *sender);
    void myBackFunc(std::shared_ptr<Message>_msg,P2PNetworkInterface *sender);


 
/**
  * @brief This function is called when a module is tapped by the user. Prints a message to the console by default.
     Can be overloaded in the user blockCode
  * @param face face that has been tapped
  */
    void onTap(int face) override;
    string onInterfaceDraw() override;

/*****************************************************************************/
/** needed to associate code to module                                      **/
	static BlockCode *buildNewBlockCode(BuildingBlock *host) {
	    return(new TpHexagonCode((HexanodesBlock*)host));
	}
/*****************************************************************************/
};

#endif /* TpHexagonCode_H_ */