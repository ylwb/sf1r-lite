/**
 * @file MasterNotifier.h
 * @author Zhongxia Li
 * @date Sep 1, 2011
 * @brief 
 */
#ifndef AGGREGATOR_NOTIFIER_H_
#define AGGREGATOR_NOTIFIER_H_

#include <string>
#include <vector>

#include <3rdparty/msgpack/msgpack.hpp>
#include <3rdparty/msgpack/rpc/client.h>
#include <3rdparty/msgpack/rpc/request.h>
#include <3rdparty/msgpack/rpc/server.h>

#include <util/singleton.h>

namespace sf1r {

struct NotifyMSG
{
    std::string method;
    std::string collection;

    std::string error;

    MSGPACK_DEFINE(method,collection,error);
};

class MasterNotifier
{
public:
    struct Master
    {
        std::string host_;
        uint16_t port_;
    };

public:
    MasterNotifier() {}

    static MasterNotifier* get()
    {
        return izenelib::util::Singleton<MasterNotifier>::get();
    }

    bool notify(NotifyMSG& msg)
    {
        if (masterList_.empty())
        {
            return false;
        }

        try
        {
            std::vector<Master>::iterator it;
            for (it = masterList_.begin(); it != masterList_.end(); it++)
            {
                msgpack::rpc::client cli(it->host_, it->port_);
                cli.notify("notify", msg);
                cli.get_loop()->flush();
            }
        }
        catch (std::exception& e)
        {
            std::cerr<<"**Failed to nofity Master : "<<e.what()<<endl;
            return false;
        }

        return true;
    }

    void addMasterAddress(const std::string& host, uint16_t port)
    {
        Master master;
        master.host_ = host;
        master.port_ = port;

        masterList_.push_back(master);
    }

protected:
    std::vector<Master> masterList_;
};

}

#endif /* AGGREGATOR_NOTIFIER_H_ */