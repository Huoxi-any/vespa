// Copyright Verizon Media. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include "distributor_message_sender_stub.h"
#include <tests/common/dummystoragelink.h>
#include <tests/common/testhelper.h>
#include <tests/common/teststorageapp.h>
#include <vespa/storage/common/hostreporter/hostinfo.h>
#include <vespa/storage/frameworkimpl/component/distributorcomponentregisterimpl.h>
#include <vespa/storage/storageutil/utils.h>
#include <vespa/storageapi/message/state.h>
#include <vespa/storageframework/defaultimplementation/clock/fakeclock.h>

namespace storage {

namespace framework { struct TickingThreadPool; }

namespace distributor {

class TopLevelDistributor;
class DistributorMetricSet;
class DistributorNodeContext;
class DistributorStripe;
class DistributorStripeComponent;
class DistributorStripeOperationContext;
class DistributorStripePool;
class IdealStateMetricSet;
class Operation;
class BucketDBUpdater;

class TopLevelDistributorTestUtil : private DoneInitializeHandler
{
public:
    TopLevelDistributorTestUtil();
    ~TopLevelDistributorTestUtil();

    void create_links();

    void close();

    size_t stripe_of_bucket(const document::BucketId& id) const noexcept;
    size_t stripe_of_bucket(const document::Bucket& bucket) const noexcept;

    /**
     * Parses the given string to a set of node => bucket info data,
     * and inserts them as nodes in the given bucket.
     * Format:
     *   "node1=checksum/docs/size,node2=checksum/docs/size"
     */
    void add_nodes_to_stripe_bucket_db(const document::Bucket& bucket,
                                       const std::string& nodeStr);
    // As the above, but always inserts into default bucket space
    void add_nodes_to_stripe_bucket_db(const document::BucketId& id, const std::string& nodeStr);

    BucketDBUpdater& bucket_db_updater();
    const IdealStateMetricSet& total_ideal_state_metrics() const;
    const DistributorMetricSet& total_distributor_metrics() const;
    const storage::distributor::DistributorNodeContext& node_context() const;
    storage::distributor::DistributorStripeOperationContext& operation_context();

    std::vector<DistributorStripe*> distributor_stripes() const;

    bool tick(bool only_tick_top_level = false);

    const DistributorConfig& current_distributor_config() const;
    void reconfigure(const DistributorConfig&);

    framework::defaultimplementation::FakeClock& fake_clock() noexcept {
        return _node->getClock();
    }

    BucketDatabase& stripe_bucket_database(uint16_t stripe_idx); // Implicit default space only
    BucketDatabase& stripe_bucket_database(uint16_t stripe_idx, document::BucketSpace space);
    const BucketDatabase& stripe_bucket_database(uint16_t stripe_idx) const; // Implicit default space only
    const BucketDatabase& stripe_bucket_database(uint16_t stripe_idx, document::BucketSpace space) const;
    [[nodiscard]] bool all_distributor_stripes_are_in_recovery_mode() const;

    void setup_distributor(int redundancy,
                           int node_count,
                           const std::string& systemState,
                           uint32_t early_return = false,
                           bool require_primary_to_be_written = true);

    void setup_distributor(int redundancy,
                           int node_count,
                           const lib::ClusterStateBundle& state,
                           uint32_t early_return = false,
                           bool require_primary_to_be_written = true);

    void notifyDoneInitializing() override {}

    BucketDatabase::Entry get_bucket(const document::Bucket& bucket) const;
    // Gets bucket entry from default space only
    BucketDatabase::Entry get_bucket(const document::BucketId& bId) const;

    DistributorMessageSenderStub& sender() noexcept { return _sender; }
    const DistributorMessageSenderStub& sender() const noexcept { return _sender; }

    // Invokes full cluster state transition pipeline rather than directly applying
    // the state and just pretending everything has been completed.
    void receive_set_system_state_command(const vespalib::string& state_str);
    bool handle_top_level_message(const std::shared_ptr<api::StorageMessage>& msg);

protected:
    vdstestlib::DirConfig _config;
    std::unique_ptr<TestDistributorApp> _node;
    std::unique_ptr<framework::TickingThreadPool> _thread_pool;
    std::unique_ptr<DistributorStripePool> _stripe_pool;
    std::unique_ptr<TopLevelDistributor> _distributor;
    std::unique_ptr<storage::DistributorComponent> _component;
    DistributorMessageSenderStub _sender;
    DistributorMessageSenderStub _sender_down;
    HostInfo _host_info;

    struct MessageSenderImpl : public ChainedMessageSender {
        DistributorMessageSenderStub& _sender;
        DistributorMessageSenderStub& _senderDown;
        MessageSenderImpl(DistributorMessageSenderStub& up, DistributorMessageSenderStub& down)
            : _sender(up), _senderDown(down) {}

        void sendUp(const std::shared_ptr<api::StorageMessage>& msg) override {
            _sender.send(msg);
        }
        void sendDown(const std::shared_ptr<api::StorageMessage>& msg) override {
            _senderDown.send(msg);
        }
    };
    MessageSenderImpl _message_sender;
    uint32_t _num_distributor_stripes;

    void enable_distributor_cluster_state(vespalib::stringref state, bool has_bucket_ownership_transfer = false);
    void enable_distributor_cluster_state(const lib::ClusterStateBundle& state);
};

}

}
