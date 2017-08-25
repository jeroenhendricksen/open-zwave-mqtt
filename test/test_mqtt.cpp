
#include <gtest/gtest.h>
#include "node_value.h"
#include "mqtt.h"
#include "mock_manager.h"
#include "mock_mosquitto.h"

using namespace std;
using namespace OpenZWave;


class mqtt_tests: public ::testing::Test
{
protected:
    void SetUp() {
        // create 2 nodes
        node_add(1, 1);
        node_add(1, 2);
    }

    void TearDown() {
        node_remove_all();
        mock_manager_cleanup();
        mqtt_unsubscribe_all();
        mock_mosquitto_cleanup();
    }

    template<typename T>
    void ASSERT_SUBSCRIPTIONS(T runs) {
        // Check created endpoints / mosquitto library subscribe history
        auto subs = mqtt_get_endpoints();
        auto hist = mock_mosquitto_subscribe_history();
        // endpoints equals created subscriptions
        ASSERT_EQ(subs.size(), hist.size());
        // Check each endpoint to match run
        for (auto it = runs.begin(); it != runs.end(); ++it) {
            auto it2 = subs.find(it->first.first);
            auto it3 = subs.find(it->first.second);
            ASSERT_TRUE(it2 != subs.end());
            ASSERT_TRUE(it3 != subs.end());
            ASSERT_EQ(it->second.GetId(), it2->second.GetId());
            ASSERT_EQ(it->second.GetId(), it3->second.GetId());
        }
        // One more cross check - subscription history to match endpoints
        for (size_t i = 0; i < hist.size(); i++) {
            ASSERT_TRUE(subs.find(hist[i]) != subs.end());
        }
    }

    template<typename T>
    void ASSERT_PUBLICATIONS(T runs) {
        // Check publication count
        auto hist = mock_mosquitto_publish_history();
        ASSERT_EQ(runs.size() * 2, hist.size());
        // Create temporary map of topics -> values.
        // This is limited to have only one message per topic, which is OK for tests
        map<string, string> topic_payload;
        for (auto it = runs.begin(); it != runs.end(); ++it) {
            string payload;
            OpenZWave::Manager::Get()->GetValueAsString(it->first, &payload);
            auto topics = it->second;
            topic_payload[topics.first] = payload;
            topic_payload[topics.second] = payload;
        }
        // Ensure that publication history is equal to runs
        for (auto it = hist.begin(); it != hist.end(); ++it) {
            const string& topic = (*it).first;
            const string& val = (*it).second;
            ASSERT_TRUE(topic_payload.find(topic) != topic_payload.end()) << "Not found: " << topic;
            ASSERT_EQ(val, topic_payload.find(topic)->second);
        }
    }
};


TEST_F(mqtt_tests, subscribe)
{
    // path: prefix/node_location/node_name/command_class_name
    // path: prefix/node_id/command_class_id
    // path -> (homeId, nodeId, genre, command_class, instance, index, type)
    map<pair<string, string>, const ValueID> runs = {
        // regular value
        {
            {"location_h1_n1/name_h1_n1/basic/label1", "1/32/1"},
            ValueID(1, 1, ValueID::ValueGenre_User, 0x20, 1, 1, ValueID::ValueType_Int)
        },
        {
            {"location_h1_n2/name_h1_n2/meter/label1", "2/50/1"},
            ValueID(1, 2, ValueID::ValueGenre_User, 0x32, 1, 1, ValueID::ValueType_Int)
        },
        // multi instance
        {
            {"location_h1_n1/name_h1_n1/switch_binary/1/label1", "1/37/1/1"},
            ValueID(1, 1, ValueID::ValueGenre_User, 0x25, 1, 1, ValueID::ValueType_Int)
        },
        {
            {"location_h1_n1/name_h1_n1/switch_binary/2/label1", "1/37/2/1"},
            ValueID(1, 1, ValueID::ValueGenre_User, 0x25, 2, 1, ValueID::ValueType_Int)
        },
        {
            {"location_h1_n1/name_h1_n1/switch_multilevel/1/label1", "1/38/1/1"},
            ValueID(1, 1, ValueID::ValueGenre_User, 0x26, 1, 1, ValueID::ValueType_Int)
        },
        {
            {"location_h1_n1/name_h1_n1/switch_multilevel/2/label1", "1/38/2/1"},
            ValueID(1, 1, ValueID::ValueGenre_User, 0x26, 2, 1, ValueID::ValueType_Int)
        },
    };

    // subscribe
    for (auto it = runs.begin(); it != runs.end(); ++it) {
        mqtt_subscribe("", it->second);
    }

    // check subscriptions
    ASSERT_SUBSCRIPTIONS(runs);
}

TEST_F(mqtt_tests, subscribe_readonly)
{
    // path: prefix/node_location/node_name/command_class_name
    // path: prefix/node_id/command_class_id
    // path -> (homeId, nodeId, genre, command_class, instance, index, type)
    map<pair<string, string>, const ValueID> runs = {
        {
            {"location_h1_n1/name_h1_n1/basic/label1", "1/32/1"},
            ValueID(1, 1, ValueID::ValueGenre_User, 0x20, 1, 1, ValueID::ValueType_Int)
        },
        {
            {"location_h1_n2/name_h1_n2/meter/label1", "2/50/1"},
            ValueID(1, 2, ValueID::ValueGenre_User, 0x32, 1, 1, ValueID::ValueType_Int)
        },
    };

    // subscribe to read only values
    for (auto it = runs.begin(); it != runs.end(); ++it) {
        mock_manager_set_value_readonly(it->second);
        mqtt_subscribe("", it->second);
    }

    // there should be no subscriptions - all values are readonly
    ASSERT_TRUE(mqtt_get_endpoints().empty());
}

TEST_F(mqtt_tests, prefix)
{
    // path: prefix/node_location/node_name/command_class_name
    // path: prefix/node_id/command_class_id
    // path -> (homeId, nodeId, genre, command_class, instance, index, type)
    map<pair<string, string>, const ValueID> runs = {
        {
            {"prefix/location_h1_n1/name_h1_n1/basic/label1", "prefix/1/32/1"},
            ValueID(1, 1, ValueID::ValueGenre_User, 0x20, 1, 1, ValueID::ValueType_Int)
        },
        {
            {"prefix/location_h1_n1/name_h1_n1/switch_binary/1/label1", "prefix/1/37/1/1"},
            ValueID(1, 1, ValueID::ValueGenre_User, 0x25, 1, 1, ValueID::ValueType_Int)
        },
    };

    // subscribe to read only values
    for (auto it = runs.begin(); it != runs.end(); ++it) {
        mqtt_subscribe("prefix", it->second);
    }

    // Check subscriptions
    ASSERT_SUBSCRIPTIONS(runs);
}

TEST_F(mqtt_tests, publish)
{
    // valueID -> (<topic1, topic2> -> payload)
    map<const ValueID, pair<string, string> > runs = {
        // regular value
        {
            ValueID(1, 1, ValueID::ValueGenre_User, 0x20, 1, 1, ValueID::ValueType_Int),
            {"location_h1_n1/name_h1_n1/basic/label1", "1/32/1"}
        },
        {
            ValueID(1, 2, ValueID::ValueGenre_User, 0x32, 1, 1, ValueID::ValueType_Int),
            {"location_h1_n2/name_h1_n2/meter/label1", "2/50/1"}
        },
        // multi instance
        {
            ValueID(1, 1, ValueID::ValueGenre_User, 0x25, 1, 1, ValueID::ValueType_Int),
            {"location_h1_n1/name_h1_n1/switch_binary/1/label1", "1/37/1/1"}
        },
        {
            ValueID(1, 1, ValueID::ValueGenre_User, 0x25, 2, 1, ValueID::ValueType_Int),
            {"location_h1_n1/name_h1_n1/switch_binary/2/label1", "1/37/2/1"}
        }
    };

    // Publish values
    for (auto it = runs.begin(); it != runs.end(); ++it) {
        mqtt_publish("", it->first);
    }

    ASSERT_PUBLICATIONS(runs);
}
