/**
 * @brief GPS RTK plugin
 * @file gps_rtk.cpp
 * @author Alexis Paques <alexis.paques@gmail.com>
 *
 * @addtogroup plugin
 * @{
 */
/*
 * Copyright 2018 Nuno Marques.
 *
 * This file is part of the mavros package and subject to the license terms
 * in the top-level LICENSE file of the mavros repository.
 * https://github.com/mavlink/mavros/tree/master/LICENSE.md
 */

#include <mavros/mavros_plugin.h>
#include <mavros_msgs/RTCM.h>
#include <algorithm>

namespace mavros {
namespace std_plugins {
/**
 * @brief GPS RTK plugin
 *
 * Publish the RTCM messages from ROS to the Pixhawk
 */
class GpsRtkPlugin : public plugin::PluginBase {
public:
	GpsRtkPlugin() : PluginBase(),
		gps_rtk_nh("~gps_rtk")
	{ }

	void initialize(UAS &uas_)
	{
		PluginBase::initialize(uas_);
		gps_rtk_sub = gps_rtk_nh.subscribe("send_rtcm", 10, &GpsRtkPlugin::rtk_cb, this);
	}

	Subscriptions get_subscriptions()
	{
		return {};
	}

private:
	ros::NodeHandle gps_rtk_nh;
	ros::Subscriber gps_rtk_sub;

	void rtk_cb(const mavros_msgs::RTCM::ConstPtr &msg)
	{
		// const int max_frag_len = mavlink::common::msg::GPS_RTCM_DATA::data::size
		// ‘mavlink::common::msg::GPS_RTCM_DATA::data’ is not a class, namespace, or enumeration
		mavlink::common::msg::GPS_RTCM_DATA rtcm_data;
		const int max_frag_len = rtcm_data.data.size();

		uint8_t seq_u5 = uint8_t(msg->header.seq & 0x1F) << 3;

		if (msg->data.size() > 4 * max_frag_len) {
			ROS_FATAL("gps_rtk: RTCM message received is bigger than the maximal possible size.");
			return;
		}

		auto data_it = msg->data.begin();
		auto end_it = msg->data.end();

		if (msg->data.size() <= max_frag_len) {
			rtcm_data.len = msg->data.size();
			rtcm_data.flags = seq_u5;
			std::copy(data_it, end_it, rtcm_data.data.begin());
			std::fill(rtcm_data.data.begin() + rtcm_data.len, rtcm_data.data.end(), 0);
			UAS_FCU(m_uas)->send_message(rtcm_data);
		} else {
			for (uint8_t fragment_id = 0; fragment_id < 4 && data_it < end_it; fragment_id++) {
				mavlink::common::msg::GPS_RTCM_DATA rtcm_data_frag;	// Can we send rtcm_data 4 times with different data safely?

				uint8_t len = std::min((int)std::distance(data_it, end_it), max_frag_len);
				rtcm_data_frag.flags = 1;				// LSB set indicates message is fragmented
				rtcm_data_frag.flags |= fragment_id++ << 1;		// Next 2 bits are fragment id
				rtcm_data_frag.flags |= seq_u5;		// Next 5 bits are sequence id
				rtcm_data_frag.len = len;

				std::copy(data_it, data_it + len, rtcm_data_frag.data.begin());
				std::fill(rtcm_data_frag.data.begin() + len, rtcm_data_frag.data.end(), 0);
				UAS_FCU(m_uas)->send_message(rtcm_data_frag);
				std::advance(data_it, len);
			}
		}
	}
};
}	// namespace std_plugins
}	// namespace mavros

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(mavros::std_plugins::GpsRtkPlugin, mavros::plugin::PluginBase)
