#include "TpHexagonCode.hpp"

TpHexagonCode::TpHexagonCode(HexanodesBlock *host):HexanodesBlockCode(host),node(host) {
    // @warning Do not remove block below, as a blockcode with a NULL host might be created
    //  for command line parsing
    if (not host) return;

    // Registers a callback (myMsgFunc) to the message of type S

    addMessageEventFunc2(BROADCAST_MSG_ID,
                       std::bind(&TpHexagonCode::myBroadcastFunc,this,
                       std::placeholders::_1, std::placeholders::_2));
    addMessageEventFunc2(FORECAST_MSG_ID,
                       std::bind(&TpHexagonCode::myForecastFunc,this,
                       std::placeholders::_1, std::placeholders::_2));
    addMessageEventFunc2(GO_MSG_ID,
                       std::bind(&TpHexagonCode::myGoFunc,this,
                       std::placeholders::_1, std::placeholders::_2));
    addMessageEventFunc2(BACK_MSG_ID,
                       std::bind(&TpHexagonCode::myBackFunc,this,
                       std::placeholders::_1, std::placeholders::_2));

}

void TpHexagonCode::startup() {

    HexanodesWorld *wrl = Hexanodes::getWorld();
    console << "start " << node->blockId << "\n";
    vector<HexanodesMotion*> tab = Hexanodes::getWorld()->getAllMotionsForModule(node);
    console << "#motion=" << tab.size() << "\n";
    int n=0;
    for (auto motion:tab) {
      Cell3DPosition destination = motion->getFinalPos(node->position);
      if (lattice->isInGrid(destination)) n++;
    }

    node->setColor(n==0?GREY:(n==1?ORANGE:GREEN));
    if (target && target->isInTarget(node->position)) {
      node->setColor(RED);
      inTarget = true;
    }
    initial = node->position;
    if (isLeader){
        parent_id = -1;
        myDistance = 0;
        round = 0;
        if(inTarget){
            //applying BFS to elect another leader
            for(auto neighbor: node->getP2PNetworkInterfaces()){
                if (neighbor->getConnectedBlockId() != -1){
                    to_send.push_back(neighbor);
                    waiting_from.push_back(neighbor);
                }
            }
            sendMessageToAllNeighbors("GO",
            new MessageOf<pair<int,int>>(GO_MSG_ID,make_pair(myDistance,round)),1000,0,0);
            
        }else{
            if(n>0 && node->getNbNeighbors() <= 2) {  
                HexanodesMotion* motion;
                vector<HexanodesMotion*> tab = wrl->getAllMotionsForModule(node);
                vector<HexanodesMotion*>::const_iterator ci=tab.begin();
                while (ci!=tab.end() ) {
                    if((*ci)->direction==motionDirection::CW)
                        motion = (*ci);
                    ci++;
                }
                if (motion != nullptr) {
                    node->setColor(CYAN);
                    auto orient = motion->getFinalOrientation(node->orientationCode);
                    Cell3DPosition destination = motion->getFinalPos(node->position);
                    scheduler->schedule(new HexanodesMotionStartEvent(scheduler->now()+SPEED, node,destination,orient));
                    nMotions++;
                }
            }else{
                //applying BFS to elect another leader
                for(auto neighbor: node->getP2PNetworkInterfaces()){
                    if (neighbor->getConnectedBlockId() != -1){
                        to_send.push_back(neighbor);
                        waiting_from.push_back(neighbor);
                    }
                }
                sendMessageToAllNeighbors("GO",
                new MessageOf<pair<int,int>>(GO_MSG_ID,make_pair(myDistance,round)),1000,0,0);
                
            }
        }
    }else {
        myDistance=100000;
    }

}

void TpHexagonCode::onMotionEnd() {

    HexanodesWorld *wrl = Hexanodes::getWorld();

    HexanodesMotion* motion;
    vector<HexanodesMotion*> tab = wrl->getAllMotionsForModule(node);
    vector<HexanodesMotion*>::const_iterator ci=tab.begin();
    while (ci!=tab.end() ) {
        if((*ci)->direction==motionDirection::CW)
            motion = (*ci);
        ci++;
    }
    
    
    if (target && target->isInTarget(node->position) && otherPossibility) {
        node->setColor(RED);
        inTarget = true;
        if(node->getNbNeighbors()>2)
            otherPossibility = false;
        else{
            if (motion != nullptr) {
                Cell3DPosition destination = motion->getFinalPos(node->position);
                if(target->isInTarget(destination))
                    otherPossibility = true;
                else
                    otherPossibility = false;
            }
        }
    }else
       node->setColor(CYAN);
    
    if((!inTarget || otherPossibility) && shouldContinue){
        if (motion != nullptr) {
            auto orient = motion->getFinalOrientation(node->orientationCode);
            Cell3DPosition destination = motion->getFinalPos(node->position);
            if ( (lattice->isInGrid(destination)) ){
                scheduler->schedule(new HexanodesMotionStartEvent(scheduler->now()+SPEED, node,destination,orient));
                nMotions++;
                if(initial == destination)
                    shouldContinue = false;
            }else{
                shouldContinue = false;
            }
            
            
        }
    
    }else{

        // elect another leader
        //applying BFS to elect another leader
        if(shouldContinue){
            parent = nullptr;
            children.clear();
            to_send.clear();
            waiting_from.clear();
            currentChildId = 0;
            myDistance = 0;
            for(auto neighbor: node->getP2PNetworkInterfaces()){
                if (neighbor->getConnectedBlockId() != -1){
                    to_send.push_back(neighbor);
                    waiting_from.push_back(neighbor);
                }
            }
            
            round++;
            sendMessageToAllNeighbors("GO",
            new MessageOf<pair<int,int>>(GO_MSG_ID,make_pair(myDistance,round)),1000,0,0);
            
        }
        
    }

    
    

}

void TpHexagonCode::myGoFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
    
    /*
    0 is stop
    1 is continue
    -1 is no
    */

    MessageOf<pair<int,int>>* msg = static_cast<MessageOf<pair<int,int>>*>(_msg.get());
    pair<int,int> pairdata = *msg->getData();
    int msgData = pairdata.first;
    int newRound = pairdata.second;
    
    console << "rcv " << msgData << " from " << sender->getConnectedBlockBId() << "\n";

    if (newRound>round){
        round = newRound;
        currentChildId = 0;
        parent_id = 0;
        isLeader = false;
        myDistance = 100000;
        parent = nullptr;
        children.clear();
        to_send.clear();
        waiting_from.clear();
    }

    if(parent == nullptr){
        myDistance = msgData + 1;
        parent = sender;
        for( auto neighbor: node->getP2PNetworkInterfaces()){
            if(neighbor != sender && neighbor->getConnectedBlockId() != -1){
                to_send.push_back(neighbor);
            }
        }
        if(to_send.size()==0){
            sendMessage("BACK",new MessageOf<int>(BACK_MSG_ID,0),sender,1000,0);
        }
        else {
            sendMessage("BACK",new MessageOf<int>(BACK_MSG_ID,1),sender,1000,0);
        }
    }else{
        if(parent == sender){
            for(auto k: to_send){
                sendMessage("GO",new MessageOf<pair<int,int>>(GO_MSG_ID,make_pair(myDistance,round)),k,1000,0);
            }
            waiting_from = to_send;
        }else{
            sendMessage("BACK",new MessageOf<int>(BACK_MSG_ID,-1),sender,1000,0);
        }
    }
        
}

void TpHexagonCode::myBackFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
    /*
    0 is stop
    1 is continue
    -1 is no
    */
    MessageOf<int>* msg = static_cast<MessageOf<int>*>(_msg.get());
    int msgData = *msg->getData();

    waiting_from = removeElement(sender,waiting_from);
    
    if (msgData == 0 || msgData == 1){
        bool exist = false;
        auto it = children.begin();
        while (!exist && it != children.end()){
            if (*it == sender)
                exist = true;
            else it++;
        }
        if (!exist)
            children.push_back(sender);
    }
    if (msgData == 0 || msgData == -1){
        to_send = removeElement(sender,to_send);
    }
    if(to_send.size() == 0){
        if(parent_id == -1){
            
            console << "\n\n\nThe Tree is built" << "\n\n\n";
            console << "BFS list : " << "\n\n";
            console << " node " << node->blockId<<"'s children are :\n";
            for(auto child: children){
                console << child->getConnectedBlockId()<<" ";
            }
            console << "\n";
            currentChildId = 0;
            if(currentChildId < children.size()){
                sendMessage("BROADCAST TO CHILD",new MessageOf<int>(BROADCAST_MSG_ID,myDistance+1),children[currentChildId],1000,0);
            }


        }
        else{
            sendMessage("BACK",new MessageOf<int>(BACK_MSG_ID,0),parent,1000,0);
        }
    }
    else{
        if (waiting_from.size() == 0) {
            if (parent_id == -1){
                  
                for(auto k: to_send){
                    sendMessage("GO",new MessageOf<pair<int,int>>(GO_MSG_ID,make_pair(myDistance,round)),k,1000,0);
                }
                waiting_from = to_send;
            }else {
                sendMessage("BACK",new MessageOf<int>(BACK_MSG_ID,1),parent,1000,0);
            }
        }
    }
}

void TpHexagonCode::myBroadcastFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {
    MessageOf<int>* msg = static_cast<MessageOf<int>*>(_msg.get());
    int msgData = *msg->getData();
    
    if(!inTarget)
        node ->setColor(msgData);
    myDistance = msgData;
    console << " node " << node->blockId<<"'s children are :\n";
    for(auto child: children){
        console << child->getConnectedBlockId()<<" ";
    }
    console << "\n";
    currentChildId = 0;
    if(currentChildId < children.size()){
        sendMessage("BROADCAST TO CHILD",new MessageOf<int>(BROADCAST_MSG_ID,myDistance+1),children[currentChildId],1000,0);
    }else{
        if ((target && target->isInTarget(node->position))) {
            sendMessage("FORECAST TO PARENT",new MessageOf<int>(FORECAST_MSG_ID,0),sender,1000,0);
        }else {
            HexanodesWorld *wrl = Hexanodes::getWorld();
            console << "start " << node->blockId << "\n";
            vector<HexanodesMotion*> tab = wrl->getAllMotionsForModule(node);
            console << "#motion=" << tab.size() << "\n";
            int n=0;
            for (auto motion:tab) {
                Cell3DPosition destination = motion->getFinalPos(node->position);
                if (lattice->isInGrid(destination)) n++;
            }

            if (n == 0){
                sendMessage("FORECAST TO PARENT",new MessageOf<int>(FORECAST_MSG_ID,0),sender,1000,0);
            }
            else{
                console << " I am the new leader and I can move :\n";
                parent_id = -1;
                isLeader = true;
                node ->setColor(CYAN);
                
                HexanodesWorld *wrl = Hexanodes::getWorld();
                HexanodesMotion* motion;
                vector<HexanodesMotion*> tab = wrl->getAllMotionsForModule(node);
                vector<HexanodesMotion*>::const_iterator ci=tab.begin();
                while (ci!=tab.end() ) {
                    if((*ci)->direction==motionDirection::CW)
                        motion = (*ci);
                    ci++;
                }
                if (motion != nullptr) {
                    node->setColor(CYAN);
                    auto orient = motion->getFinalOrientation(node->orientationCode);
                    Cell3DPosition destination = motion->getFinalPos(node->position);
                    scheduler->schedule(new HexanodesMotionStartEvent(scheduler->now()+SPEED, node,destination,orient));
                    nMotions++;
                }
            }
        }
        
    }
    
    



    
}

void TpHexagonCode::myForecastFunc(std::shared_ptr<Message>_msg, P2PNetworkInterface*sender) {

    if(parent_id == -1){
        console << "I AM LEADER" <<"\n";
        currentChildId++;
        if(currentChildId < children.size()){
            sendMessage("BROADCAST TO CHILD",new MessageOf<int>(BROADCAST_MSG_ID,myDistance+1),children[currentChildId],1000,0);
        }else{
            console << "[LAST LEADER] DONE !" <<"\n";
        }
    }else{
        currentChildId++;
        if(currentChildId < children.size()){
            sendMessage("BROADCAST TO CHILD",new MessageOf<int>(BROADCAST_MSG_ID,myDistance+1),children[currentChildId],1000,0);
        }else{
            
            if ((target && target->isInTarget(node->position))) {
                sendMessage("FORECAST TO PARENT",new MessageOf<int>(FORECAST_MSG_ID,0),parent,1000,0);
            }else {
                HexanodesWorld *wrl = Hexanodes::getWorld();
                console << "start " << node->blockId << "\n";
                vector<HexanodesMotion*> tab = wrl->getAllMotionsForModule(node);
                console << "#motion=" << tab.size() << "\n";
                int n=0;
                for (auto motion:tab) {
                    Cell3DPosition destination = motion->getFinalPos(node->position);
                    if (lattice->isInGrid(destination)) n++;
                }

                if (n == 0){
                    sendMessage("FORECAST TO PARENT",new MessageOf<int>(FORECAST_MSG_ID,0),parent,1000,0);
                }
                else{
                    console << " I am the new leader and I can move:\n";
                    parent_id = -1;
                    isLeader = true;
                    node ->setColor(CYAN);

                    
                    HexanodesWorld *wrl = Hexanodes::getWorld();    
                    HexanodesMotion* motion;
                    vector<HexanodesMotion*> tab = wrl->getAllMotionsForModule(node);
                    vector<HexanodesMotion*>::const_iterator ci=tab.begin();
                    while (ci!=tab.end() ) {
                        if((*ci)->direction==motionDirection::CW)
                            motion = (*ci);
                        ci++;
                    }
                    // if an element is found, activate this motion scheduling the startEvent
                    if (motion != nullptr) {
                        node->setColor(CYAN);
                        auto orient = motion->getFinalOrientation(node->orientationCode);
                        Cell3DPosition destination = motion->getFinalPos(node->position);
                        scheduler->schedule(new HexanodesMotionStartEvent(scheduler->now()+SPEED, node,destination,orient));
                        nMotions++;
                    }
                }
            }
        }
    }
    
}


void TpHexagonCode::parseUserBlockElements(TiXmlElement *config) {
    const char *attr = config->Attribute("leader");
    if (attr!=nullptr) {
        string str(attr);
        if (str=="true" || str=="1" || str=="yes") {
            isLeader=true;
            node->setColor(RED);
            console << node->blockId << " is the Leader" << "\n"; 
        }
    }
}




void TpHexagonCode::onUserKeyPressed(unsigned char c, int x, int y) {
    switch (c) {
        case 'a' : // update with your code
        break;
    }
}

void TpHexagonCode::onTap(int face) {
    std::cout << "Block 'tapped':" << node->blockId << std::endl; // complete with your code
}

void TpHexagonCode::onBlockSelected() {
    std::cout << "Block selected" << std::endl; // complete with your code
}

string TpHexagonCode::onInterfaceDraw() {
    stringstream trace;
    trace << "Nb motions = " + to_string(nMotions)<< "";
    return trace.str(); // to update with your text
}

