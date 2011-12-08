#include "SearchMasterManager.h"
#include "SearchNodeManager.h"

#include <boost/lexical_cast.hpp>

using namespace sf1r;

SearchMasterManager::SearchMasterManager()
: masterState_(MASTER_STATE_INIT)
{
}

void SearchMasterManager::init()
{
    // initialize zookeeper
    initZooKeeper(
            SearchNodeManagerSingleton::get()->getDSUtilConfig().zkConfig_.zkHosts_,
            SearchNodeManagerSingleton::get()->getDSUtilConfig().zkConfig_.zkRecvTimeout_);

    // initialize topology info
    topology_.clusterId_ = SearchNodeManagerSingleton::get()->getDSTopologyConfig().clusterId_;
    topology_.nodeNum_ =  SearchNodeManagerSingleton::get()->getDSTopologyConfig().nodeNum_;
    topology_.shardNum_ =  SearchNodeManagerSingleton::get()->getDSTopologyConfig().shardNum_;

    curNodeInfo_ = SearchNodeManagerSingleton::get()->getNodeInfo();
}

void SearchMasterManager::start()
{
    // call once
    if (masterState_ == MASTER_STATE_INIT)
    {
        masterState_ = MASTER_STATE_STARTING;

        init();

        // Ensure connected to zookeeper
        if (!zookeeper_->isConnected())
        {
            zookeeper_->connect(true);

            if (!zookeeper_->isConnected())
            {
                masterState_ = MASTER_STATE_STARTING_WAIT_ZOOKEEPER;
                std::cout<<"[SearchMasterManager] waiting for ZooKeeper Service..."<<std::endl;
                return;
            }
        }

        doStart();
    }
}

void SearchMasterManager::stop()
{
    // stop events on exit
    zookeeper_->disconnect();
}

bool SearchMasterManager::getShardReceiver(
        unsigned int shardid,
        std::string& host,
        unsigned int& recvPort)
{
    boost::lock_guard<boost::mutex> lock(mutex_);

    WorkerMapT::iterator it = workerMap_.find(shardid);
    if (it != workerMap_.end())
    {
        host = it->second->host_;
        recvPort = it->second->dataPort_;
        return true;
    }
    else
    {
        return false;
    }
}

void SearchMasterManager::process(ZooKeeperEvent& zkEvent)
{
    std::cout << "[SearchMasterManager] "<<state2string(masterState_)<<" "<<zkEvent.toString();
    // xxx, handle all events here?

    if (zkEvent.type_ == ZOO_SESSION_EVENT && zkEvent.state_ == ZOO_CONNECTED_STATE)
    {
        if (masterState_ == MASTER_STATE_STARTING_WAIT_ZOOKEEPER)
        {
            masterState_ = MASTER_STATE_STARTING;
            doStart();
        }
    }
}

void SearchMasterManager::onNodeCreated(const std::string& path)
{
    if (masterState_ == MASTER_STATE_STARTING_WAIT_WORKERS)
    {
        // try detect workers
        masterState_ = MASTER_STATE_STARTING;
        detectWorkers();
    }
    else if (masterState_ == MASTER_STATE_STARTED
             && masterState_ != MASTER_STATE_RECOVERING)
    {
        // try recover
        recover(path);
    }
    else if (masterState_ == MASTER_STATE_STARTED
                 && masterState_ != MASTER_STATE_FAILOVERING)
    {
        // try recover
        failover(path);
    }
}

void SearchMasterManager::onNodeDeleted(const std::string& path)
{
    if (masterState_ == MASTER_STATE_STARTED)
    {
        // try failover
        failover(path);
    }
}

void SearchMasterManager::onChildrenChanged(const std::string& path)
{
    if (masterState_ > MASTER_STATE_STARTING_WAIT_ZOOKEEPER)
    {
        detectReplicaSet(path);
    }
}


void SearchMasterManager::showWorkers()
{
    WorkerMapT::iterator it;
    for (it = workerMap_.begin(); it != workerMap_.end(); it++)
    {
        cout << it->second->toString() ;
    }
}

/// private ////////////////////////////////////////////////////////////////////

void SearchMasterManager::initZooKeeper(const std::string& zkHosts, const int recvTimeout)
{
    zookeeper_.reset(new ZooKeeper(zkHosts, recvTimeout));
    zookeeper_->registerEventHandler(this);
}

std::string SearchMasterManager::state2string(MasterStateType e)
{
    std::stringstream ss;
    switch (e)
    {
    case MASTER_STATE_INIT:
        return "MASTER_STATE_INIT";
        break;
    case MASTER_STATE_STARTING:
        return "MASTER_STATE_STARTING";
        break;
    case MASTER_STATE_STARTING_WAIT_ZOOKEEPER:
        return "MASTER_STATE_STARTING_WAIT_ZOOKEEPER";
        break;
    case MASTER_STATE_STARTING_WAIT_WORKERS:
        return "MASTER_STATE_STARTING_WAIT_WORKERS";
        break;
    case MASTER_STATE_STARTED:
        return "MASTER_STATE_STARTED";
        break;
    case MASTER_STATE_FAILOVERING:
        return "MASTER_STATE_FAILOVERING";
        break;
    case MASTER_STATE_RECOVERING:
        return "MASTER_STATE_RECOVERING";
        break;
    }

    return "UNKNOWN";
}

void SearchMasterManager::watchAll()
{
    // for replica change
    std::vector<std::string> childrenList;
    zookeeper_->getZNodeChildren(NodeDef::getSF1TopologyPath(), childrenList, ZooKeeper::WATCH);
    for (size_t i = 0; i < childrenList.size(); i++)
    {
        std::vector<std::string> chchList;
        zookeeper_->getZNodeChildren(childrenList[i], chchList, ZooKeeper::WATCH);
    }

    // for nodes change
    for (uint32_t nodeid = 1; nodeid <= topology_.nodeNum_; nodeid++)
    {
        std::string nodePath = NodeDef::getNodePath(curNodeInfo_.replicaId_, nodeid);
        zookeeper_->isZNodeExists(nodePath, ZooKeeper::WATCH);
    }
}

void SearchMasterManager::doStart()
{
    detectReplicaSet();

    detectWorkers();

    // Register SF1, who works as Master, as search server without
    // waiting for all workers to be ready. Because even if one worker is
    // broken down and not recovered, other workers should be in serving.
    registerSearchServer();
}

int SearchMasterManager::detectWorkers()
{
    boost::lock_guard<boost::mutex> lock(mutex_);
    //std::cout<<"[SearchMasterManager] detecting Workers ..."<<std::endl;

    // detect workers from "current" replica
    size_t detected = 0, good = 0;
    for (uint32_t nodeid = 1; nodeid <= topology_.nodeNum_; nodeid++)
    {
        NodeData ndata;
        std::string sdata;
        std::string nodePath = NodeDef::getNodePath(curNodeInfo_.replicaId_, nodeid);
        // get node data
        if (zookeeper_->getZNodeData(nodePath, sdata, ZooKeeper::WATCH))
        {
            ndata.loadZkData(sdata);
            if (ndata.hasKey(NodeData::NDATA_KEY_WORKER_PORT))
            {
                shardid_t shardid = ndata.getUIntValue(NodeData::NDATA_KEY_SHARD_ID);
                if (shardid > 0 && shardid <= topology_.shardNum_)
                {
                    if (workerMap_.find(shardid) != workerMap_.end())
                    {
                        // check if shard id is reduplicated with existed node
                        if (workerMap_[shardid]->nodeId_ != nodeid)
                        {
                            workerMap_[shardid]->isGood_ = false;
                            std::cout <<"Error, shardid "<<shardid<<" for Node "<<nodeid
                                      <<" is the same with Node "<<workerMap_[shardid]->nodeId_<<std::endl;
                        }
                        else
                        {
                            workerMap_[shardid]->isGood_ = true;
                        }
                    }
                    else
                    {
                        // insert new worker
                        boost::shared_ptr<WorkerNode> pworkerNode(new WorkerNode);
                        pworkerNode->isGood_ = true;
                        workerMap_[shardid] = pworkerNode;
                    }

                    // set worker info
                    workerMap_[shardid]->shardId_ = shardid; // worker id
                    workerMap_[shardid]->nodeId_ = nodeid;
                    workerMap_[shardid]->replicaId_ = curNodeInfo_.replicaId_;
                    workerMap_[shardid]->host_ = ndata.getValue(NodeData::NDATA_KEY_HOST); //check validity xxx
                    //workerMap_[shardid]->workerPort_ = ndata.getUIntValue(NodeData::NDATA_KEY_WORKER_PORT);
                    try {
                        workerMap_[shardid]->workerPort_ =
                                boost::lexical_cast<shardid_t>(ndata.getValue(NodeData::NDATA_KEY_WORKER_PORT));
                    }
                    catch (std::exception& e)
                    {
                        workerMap_[shardid]->isGood_ = false;
                        std::cout <<"Error workerPort "<<ndata.getValue(NodeData::NDATA_KEY_WORKER_PORT)
                                  <<" from "<<ndata.getValue(NodeData::NDATA_KEY_HOST)<<std::endl;
                    }
                    try {
                        workerMap_[shardid]->dataPort_ =
                                boost::lexical_cast<shardid_t>(ndata.getValue(NodeData::NDATA_KEY_DATA_PORT));
                    }
                    catch (std::exception& e)
                    {
                        workerMap_[shardid]->isGood_ = false;
                        std::cout <<"Error dataPort "<<ndata.getValue(NodeData::NDATA_KEY_DATA_PORT)
                                  <<" from "<<ndata.getValue(NodeData::NDATA_KEY_HOST)<<std::endl;
                    }

                    // xxx check more info?
                    std::cout <<"[SearchMasterManager] detected "<<workerMap_[shardid]->toString()<<std::endl;
                    detected ++;
                    if (workerMap_[shardid]->isGood_)
                        good ++;
                }
                else
                {
                    std::cout <<"Error shardid "<<shardid<<" from "<<ndata.getValue(NodeData::NDATA_KEY_HOST)<<std::endl;
                    continue;
                }
            }
        }
        else
        {
            // todo, check replica node ?

            // former watch will fail if nodePath did not existed.
            zookeeper_->isZNodeExists(nodePath, ZooKeeper::WATCH);
        }
    }

    if (detected >= topology_.shardNum_)
    {
        masterState_ = MASTER_STATE_STARTED;
        std::cout<<"[SearchMasterManager] all Workers are detected "<<topology_.shardNum_
                 <<" (good "<<good<<")"<<std::endl;
    }
    else
    {
        masterState_ = MASTER_STATE_STARTING_WAIT_WORKERS;
        std::cout<<"[SearchMasterManager] detected "<<detected
                 <<" worker(s) (good "<<good<<"), all "<<topology_.shardNum_<<" - waiting"<<std::endl;
    }

    // update config
    if (good > 0)
    {
        resetAggregatorConfig();
    }

    return good;
}

void SearchMasterManager::detectReplicaSet(const std::string& zpath)
{
    //std::cout<<"[SearchMasterManager] detecting replicas ..."<<std::endl;

    // xxx synchronize
    std::vector<std::string> childrenList;
    zookeeper_->getZNodeChildren(NodeDef::getSF1TopologyPath(), childrenList, ZooKeeper::WATCH);

    replicaIdList_.clear();
    for (size_t i = 0; i < childrenList.size(); i++)
    {
        std::string sreplicaId;
        zookeeper_->getZNodeData(childrenList[i], sreplicaId);
        try {
            replicaIdList_.push_back(boost::lexical_cast<replicaid_t>(sreplicaId));
            ///std::cout<<"[SearchMasterManager] detected replica "<<replicaIdList_.back()<<std::endl;
        }
        catch (std::exception& e)
        {}

        // watch for nodes change
        std::vector<std::string> chchList;
        zookeeper_->getZNodeChildren(childrenList[i], chchList, ZooKeeper::WATCH);
        zookeeper_->isZNodeExists(childrenList[i],ZooKeeper::WATCH);
    }

    // new replica or new node in replica joined
    // xxx, notify if failover failed
    if (masterState_ == MASTER_STATE_STARTING_WAIT_WORKERS)
    {
        detectWorkers();
    }

    WorkerMapT::iterator it;
    for (it = workerMap_.begin(); it != workerMap_.end(); it++)
    {
        boost::shared_ptr<WorkerNode>& pworkerNode = it->second;
        if (!pworkerNode->isGood_)
        {
            // try failover
            failover(pworkerNode);
        }
    }
}

void SearchMasterManager::failover(const std::string& zpath)
{
    masterState_ = MASTER_STATE_FAILOVERING; //xxx, use lock

    // check path
    WorkerMapT::iterator it;
    for (it = workerMap_.begin(); it != workerMap_.end(); it++)
    {
        boost::shared_ptr<WorkerNode>& pworkerNode = it->second;
        std::string nodePath = NodeDef::getNodePath(pworkerNode->replicaId_, pworkerNode->nodeId_);
        if (zpath == nodePath)
        {
            std::cout <<"failover, node broken "<<nodePath<<std::endl;
            if (failover(pworkerNode))
            {
                std::cout <<"failover, finished "<<std::endl;
            }
            else
            {
                std::cout <<"failover, failed to cover this failure... "<<std::endl;
            }
        }
    }

    masterState_ = MASTER_STATE_STARTED;
}

bool SearchMasterManager::failover(boost::shared_ptr<WorkerNode>& pworkerNode)
{
    pworkerNode->isGood_ = false;
    for (size_t i = 0; i < replicaIdList_.size(); i++)
    {
        if (replicaIdList_[i] != pworkerNode->replicaId_)
        {
            // try switch to replicaIdList_[i]
            NodeData ndata;
            std::string sdata;
            std::string nodePath = NodeDef::getNodePath(replicaIdList_[i], pworkerNode->nodeId_);
            // get node data
            if (zookeeper_->getZNodeData(nodePath, sdata, ZooKeeper::WATCH))
            {
                ndata.loadZkData(sdata);
                shardid_t shardid = ndata.getUIntValue(NodeData::NDATA_KEY_SHARD_ID);
                if (shardid == pworkerNode->shardId_)
                {
                    std::cout<<"switching node "<<pworkerNode->nodeId_<<" from replica "<<pworkerNode->replicaId_
                             <<" to "<<replicaIdList_[i]<<std::endl;
                    // xxx,
                    //pworkerNode->shardId_ = shardid;
                    //pworkerNode->nodeId_ = nodeid;
                    pworkerNode->replicaId_ = replicaIdList_[i]; // new replica
                    pworkerNode->host_ = ndata.getValue(NodeData::NDATA_KEY_HOST); //check validity xxx
                    pworkerNode->workerPort_ = ndata.getUIntValue(NodeData::NDATA_KEY_WORKER_PORT);
                    try {
                        pworkerNode->workerPort_ =
                                boost::lexical_cast<shardid_t>(ndata.getValue(NodeData::NDATA_KEY_WORKER_PORT));
                    }
                    catch (std::exception& e)
                    {
                        std::cout <<"Error workerPort "<<ndata.getValue(NodeData::NDATA_KEY_WORKER_PORT)
                                  <<" from "<<ndata.getValue(NodeData::NDATA_KEY_HOST)<<std::endl;
                        continue;
                    }

                    pworkerNode->isGood_ = true;
                    break;
                }
                else
                {
                    //xxx
                    std::cerr<<"Error, replica inconsistent, Replica "<<replicaIdList_[i]
                             <<" Node "<<pworkerNode->nodeId_<<" has shard id "<<pworkerNode->shardId_
                             <<", but expected shard id is "<<pworkerNode->shardId_<<std::endl;
                }
            }
        }
    }

    // notify aggregators
    if (pworkerNode->isGood_)
    {
        resetAggregatorConfig();
    }

    // Watch, waiting for node recover from current replica,
    // xxx, watch all replicas, if failed and current not recovered, but another replica may recover
    zookeeper_->isZNodeExists(
            NodeDef::getNodePath(curNodeInfo_.replicaId_, pworkerNode->nodeId_),
            ZooKeeper::WATCH);

    return pworkerNode->isGood_;
}


void SearchMasterManager::recover(const std::string& zpath)
{
    masterState_ = MASTER_STATE_RECOVERING; // lock?

    WorkerMapT::iterator it;
    for (it = workerMap_.begin(); it != workerMap_.end(); it++)
    {
        boost::shared_ptr<WorkerNode>& pworkerNode = it->second;
        if (zpath == NodeDef::getNodePath(curNodeInfo_.replicaId_, pworkerNode->nodeId_))
        {
            std::cout<<"Rcovering, node "<<pworkerNode->nodeId_<<" recovered in current replica "
                     <<curNodeInfo_.replicaId_<<std::endl;

            NodeData ndata;
            std::string sdata;
            if (zookeeper_->getZNodeData(zpath, sdata, ZooKeeper::WATCH))
            {
                // xxx recover

                ndata.loadZkData(sdata);
                //pworkerNode->shardId_ = shardid;
                //pworkerNode->nodeId_ = nodeid;
                pworkerNode->replicaId_ = curNodeInfo_.replicaId_; // new replica
                pworkerNode->host_ = ndata.getValue(NodeData::NDATA_KEY_HOST); //check validity xxx
                try {
                    pworkerNode->workerPort_ =
                            boost::lexical_cast<shardid_t>(ndata.getValue(NodeData::NDATA_KEY_WORKER_PORT));
                }
                catch (std::exception& e)
                {
                    std::cout <<"Error workerPort "<<ndata.getValue(NodeData::NDATA_KEY_WORKER_PORT)
                              <<" from "<<ndata.getValue(NodeData::NDATA_KEY_HOST)<<std::endl;
                    break;
                }

                // notify aggregators
                pworkerNode->isGood_ = true;
                resetAggregatorConfig();
                break;
            }
        }
    }

    masterState_ = MASTER_STATE_STARTED;
}

void SearchMasterManager::registerSearchServer()
{
    // This Master is ready to serve
    std::string path = NodeDef::getSF1ServicePath();

    if (!zookeeper_->isZNodeExists(path))
    {
        zookeeper_->createZNode(path);
    }

    std::stringstream serverAddress;
    serverAddress<<curNodeInfo_.host_<<":"<<curNodeInfo_.baPort_;
    if (zookeeper_->createZNode(path+"/Server", serverAddress.str(), ZooKeeper::ZNODE_EPHEMERAL_SEQUENCE))
    {
        serverRealPath_ = zookeeper_->getLastCreatedNodePath();

        // std::cout << "[SearchMasterManager] Master ready to serve -- "<<serverRealPath_<<std::endl;//
    }
}

void SearchMasterManager::deregisterSearchServer()
{
    zookeeper_->deleteZNode(serverRealPath_);

    std::cout << "[SearchMasterManager] Master is boken down... " <<std::endl;
}

void SearchMasterManager::resetAggregatorConfig()
{
    //std::cout << "[SearchMasterManager] set config for "<<aggregatorList_.size()<<" aggregators"<<std::endl;

    aggregatorConfig_.reset();

    WorkerMapT::iterator it;
    for (it = workerMap_.begin(); it != workerMap_.end(); it++)
    {
        boost::shared_ptr<WorkerNode>& pworkerNode = it->second;
        if (pworkerNode->isGood_)
        {
            bool isLocal = (pworkerNode->nodeId_ == curNodeInfo_.nodeId_);
            aggregatorConfig_.addWorker(pworkerNode->host_, pworkerNode->workerPort_, pworkerNode->shardId_, isLocal);
        }
    }

    std::cout << aggregatorConfig_.toString();

    // set aggregator configuration
    std::vector<net::aggregator::AggregatorBase*>::iterator agg_it;
    for (agg_it = aggregatorList_.begin(); agg_it != aggregatorList_.end(); agg_it++)
    {
        (*agg_it)->setAggregatorConfig(aggregatorConfig_);
    }
}
