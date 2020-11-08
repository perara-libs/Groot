#ifndef BT_ZMQ_PUBLISHER_H
#define BT_ZMQ_PUBLISHER_H

#include <array>
#include <future>
#include "abstract_logger.h"

using namespace BT;

class CustomPublisherZMQ : public StatusChangeLogger
{
    static std::atomic<bool> ref_count;

  public:
    CustomPublisherZMQ(
                 unsigned max_msg_per_second = 100,
                 unsigned publisher_port = 1666,
                 unsigned server_port = 1667);

    void setupTree(const BT::Tree *tree);
    virtual ~CustomPublisherZMQ();

  private:
    virtual void callback(Duration timestamp, const TreeNode& node, NodeStatus prev_status,
                          NodeStatus status) override;

    virtual void flush() override;

    const BT::Tree* tree_ = NULL;
    std::vector<uint8_t> tree_buffer_;
    std::vector<uint8_t> status_buffer_;
    std::vector<SerializedTransition> transition_buffer_;
    std::chrono::milliseconds min_time_between_msgs_;

    std::atomic_bool active_server_;
    std::thread thread_;

    void createStatusBuffer();

    TimePoint deadline_;
    std::mutex mutex_;
    std::atomic_bool send_pending_;

    std::future<void> send_future_;

    struct Pimpl;
    Pimpl* zmq_;

};


#endif   // BT_ZMQ_PUBLISHER_H
