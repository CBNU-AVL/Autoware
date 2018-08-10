#include <stdio.h>
#include <cmath>

#include <geometry_msgs/PoseStamped.h>
#include <jsk_recognition_msgs/BoundingBoxArray.h>
#include <jsk_rviz_plugins/OverlayText.h>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <std_msgs/UInt8.h>

#include <autoware_msgs/CloudClusterArray.h>
#include <autoware_msgs/lane.h>
#include <autoware_msgs/traffic_light.h>

#include <cross_road_area.hpp>
#include <decision_maker_node.hpp>
#include <state_machine_lib/state.hpp>
#include <state_machine_lib/state_context.hpp>

namespace decision_maker
{
void DecisionMakerNode::callbackFromFilteredPoints(const sensor_msgs::PointCloud2::ConstPtr& msg)
{
  setEventFlag("received_pointcloud_for_NDT", true);

  /* todo */
  /* create timer for flags reset   */
}

void DecisionMakerNode::callbackFromSimPose(const geometry_msgs::PoseStamped& msg)
{
  ROS_INFO("Received system is going to simulation mode");
  // handleStateCmd(state_machine::DRIVE_STATE);
  Subs["sim_pose"].shutdown();
}

void DecisionMakerNode::callbackFromStateCmd(const std_msgs::String& msg)
{
  //  ROS_INFO("Received State Command");
  tryNextState(msg.data);
  // handleStateCmd((uint64_t)1ULL << (uint64_t)msg.data);
}

void DecisionMakerNode::callbackFromLaneChangeFlag(const std_msgs::Int32& msg)
{
#if 0
  if (msg.data == enumToInteger<E_ChangeFlags>(E_ChangeFlags::LEFT) &&
      ctx->isCurrentState(state_machine::DRIVE_BEHAVIOR_ACCEPT_LANECHANGE_STATE))
  {
    ctx->disableCurrentState(state_machine::DRIVE_BEHAVIOR_LANECHANGE_RIGHT_STATE);
    ctx->setCurrentState(state_machine::DRIVE_BEHAVIOR_LANECHANGE_LEFT_STATE);
  }
  else if (msg.data == enumToInteger<E_ChangeFlags>(E_ChangeFlags::RIGHT) &&
           ctx->isCurrentState(state_machine::DRIVE_BEHAVIOR_ACCEPT_LANECHANGE_STATE))
  {
    ctx->disableCurrentState(state_machine::DRIVE_BEHAVIOR_LANECHANGE_LEFT_STATE);
    ctx->setCurrentState(state_machine::DRIVE_BEHAVIOR_LANECHANGE_RIGHT_STATE);
  }
  else
  {
    ctx->disableCurrentState(state_machine::DRIVE_BEHAVIOR_LANECHANGE_RIGHT_STATE);
    ctx->disableCurrentState(state_machine::DRIVE_BEHAVIOR_LANECHANGE_LEFT_STATE);
  }
#endif
}

void DecisionMakerNode::callbackFromConfig(const autoware_msgs::ConfigDecisionMaker& msg)
{
  ROS_INFO("Param setted by Runtime Manager");
  enableDisplayMarker = msg.enable_display_marker;
  param_num_of_steer_behind_ = msg.num_of_steer_behind;
}

void DecisionMakerNode::callbackFromLightColor(const ros::MessageEvent<autoware_msgs::traffic_light const>& event)
{
#if 0
  const autoware_msgs::traffic_light *light = event.getMessage().get();
  //  const ros::M_string &header = event.getConnectionHeader();
  //  std::string topic = header.at("topic");

  if (!isManualLight)
  {  // && topic.find("manage") == std::string::npos){
    current_traffic_light_ = light->traffic_light;
    if (current_traffic_light_ == state_machine::E_RED || current_traffic_light_ == state_machine::E_YELLOW)
    {
      ctx->setCurrentState(state_machine::DRIVE_BEHAVIOR_TRAFFICLIGHT_RED_STATE);
      ctx->disableCurrentState(state_machine::DRIVE_BEHAVIOR_TRAFFICLIGHT_GREEN_STATE);
    }
    else
    {
      ctx->setCurrentState(state_machine::DRIVE_BEHAVIOR_TRAFFICLIGHT_GREEN_STATE);
      ctx->disableCurrentState(state_machine::DRIVE_BEHAVIOR_TRAFFICLIGHT_RED_STATE);
    }
  }
#endif
}

void DecisionMakerNode::callbackFromObjectDetector(const autoware_msgs::CloudClusterArray& msg)
{
#if 0
  // This function is a quick hack implementation.
  // If detection result exists in DetectionArea, decisionmaker sets object
  // detection
  // flag(foundOthervehicleforintersectionstop).
  // The flag is referenced in the stopline state, and if it is true it will
  // continue to stop.

  static double setFlagTime = 0.0;
  bool l_detection_flag = false;
  if (ctx->isCurrentState(state_machine::DRIVE_STATE))
  {
    if (msg.clusters.size())
    {
      // if euclidean_cluster does not use wayarea, it may always founded.
      for (const auto cluster : msg.clusters)
      {
        geometry_msgs::PoseStamped cluster_pose;
        geometry_msgs::PoseStamped baselink_pose;
        cluster_pose.pose = cluster.bounding_box.pose;
        cluster_pose.header = cluster.header;

        tflistener_baselink.transformPose(cluster.header.frame_id, cluster.header.stamp, cluster_pose, "base_link",
                                          baselink_pose);

        if (detectionArea_.x1 * param_detection_area_rate_ >= baselink_pose.pose.position.x &&
            baselink_pose.pose.position.x >= detectionArea_.x2 * param_detection_area_rate_ &&
            detectionArea_.y1 * param_detection_area_rate_ >= baselink_pose.pose.position.y &&
            baselink_pose.pose.position.y >= detectionArea_.y2 * param_detection_area_rate_)
        {
          l_detection_flag = true;
          setFlagTime = ros::Time::now().toSec();
          break;
        }
      }
    }
  }
  /* The true state continues for more than 1 second. */
  if (l_detection_flag || (ros::Time::now().toSec() - setFlagTime) >= 1.0 /*1.0sec*/)
  {
    foundOtherVehicleForIntersectionStop_ = l_detection_flag;
  }
#endif
}

void DecisionMakerNode::insertPointWithinCrossRoad(const std::vector<CrossRoadArea>& _intersects,
                                                   autoware_msgs::LaneArray& lane_array)
{
  for (auto& lane : lane_array.lanes)
  {
    for (auto& wp : lane.waypoints)
    {
      geometry_msgs::Point pp;
      pp.x = wp.pose.pose.position.x;
      pp.y = wp.pose.pose.position.y;
      pp.z = wp.pose.pose.position.z;

      for (auto& area : intersects)
      {
        if (CrossRoadArea::isInsideArea(&area, pp))
        {
          // area's
          if (area.insideLanes.empty() || wp.gid != area.insideLanes.back().waypoints.back().gid + 1)
          {
            autoware_msgs::lane nlane;
            area.insideLanes.push_back(nlane);
            area.bbox.pose.orientation = wp.pose.pose.orientation;
          }
          area.insideLanes.back().waypoints.push_back(wp);
          area.insideWaypoint_points.push_back(pp);  // geometry_msgs::point
          // area.insideLanes.Waypoints.push_back(wp);//autoware_msgs::waypoint
          // lane's wp
          wp.wpstate.aid = area.area_id;
        }
      }
    }
  }
}

inline double getDistance(double ax, double ay, double bx, double by)
{
  return std::hypot(ax - bx, ay - by);
}

void DecisionMakerNode::setWaypointState(autoware_msgs::LaneArray& lane_array)
{
  insertPointWithinCrossRoad(intersects, lane_array);
  // STR
  bool existInsideLanes = false;

  for (auto& area : intersects)
  {
    for (auto& laneinArea : area.insideLanes)
    {
      existInsideLanes = true;
      // To straight/left/right recognition by using angle
      // between first-waypoint and end-waypoint in intersection area.
      int angle_deg = ((int)std::floor(calcIntersectWayAngle(laneinArea)));  // normalized
      int steering_state;

      if (angle_deg <= ANGLE_LEFT)
        steering_state = autoware_msgs::WaypointState::STR_LEFT;
      else if (angle_deg >= ANGLE_RIGHT)
        steering_state = autoware_msgs::WaypointState::STR_RIGHT;
      else
        steering_state = autoware_msgs::WaypointState::STR_STRAIGHT;

      for (auto& wp_lane : laneinArea.waypoints)
        for (auto& lane : lane_array.lanes)
          for (auto& wp : lane.waypoints)
            if (wp.gid == wp_lane.gid && wp.wpstate.aid == area.area_id)
            {
              wp.wpstate.steering_state = steering_state;
            }
            else if (wp.wpstate.steering_state == 0)
            {
              wp.wpstate.steering_state = autoware_msgs::WaypointState::STR_STRAIGHT;
            }
    }
  }
  if(!existInsideLanes)
    for (auto& lane : lane_array.lanes)
      for (auto& wp : lane.waypoints)
          wp.wpstate.steering_state = autoware_msgs::WaypointState::STR_STRAIGHT;

  // STOP
  std::vector<StopLine> stoplines = g_vmap.findByFilter([&](const StopLine& stopline) {
    return ((g_vmap.findByKey(Key<RoadSign>(stopline.signid)).type &
             (autoware_msgs::WaypointState::TYPE_STOP | autoware_msgs::WaypointState::TYPE_STOPLINE)) != 0);
  });

  for (auto& lane : lane_array.lanes)
  {
    for (size_t wp_idx = 0; wp_idx < lane.waypoints.size() - 1; wp_idx++)
    {
      for (auto& stopline : stoplines)
      {
        geometry_msgs::Point bp =
            VMPoint2GeoPoint(g_vmap.findByKey(Key<Point>(g_vmap.findByKey(Key<Line>(stopline.lid)).bpid)));
        geometry_msgs::Point fp =
            VMPoint2GeoPoint(g_vmap.findByKey(Key<Point>(g_vmap.findByKey(Key<Line>(stopline.lid)).fpid)));

        if (amathutils::isIntersectLine(lane.waypoints.at(wp_idx).pose.pose.position,
                                        lane.waypoints.at(wp_idx + 1).pose.pose.position, bp, fp))
        {
          geometry_msgs::Point center_point;
          center_point.x = (bp.x * 2 + fp.x) / 3;
          center_point.y = (bp.y * 2 + fp.y) / 3;
          center_point.z = (bp.z + fp.z) / 2;
          if (amathutils::isPointLeftFromLine(center_point, lane.waypoints.at(wp_idx).pose.pose.position,
                                              lane.waypoints.at(wp_idx + 1).pose.pose.position) >= 0)
          {
            center_point.x = (bp.x + fp.x) / 2;
            center_point.y = (bp.y + fp.y) / 2;
            geometry_msgs::Point interpolation_point =
                amathutils::getNearPtOnLine(center_point, lane.waypoints.at(wp_idx).pose.pose.position,
                                            lane.waypoints.at(wp_idx + 1).pose.pose.position);

            autoware_msgs::waypoint wp = lane.waypoints.at(wp_idx);
            wp.wpstate.stop_state = g_vmap.findByKey(Key<RoadSign>(stopline.signid)).type;
            wp.pose.pose.position.x = interpolation_point.x;
            wp.pose.pose.position.y = interpolation_point.y;
            wp.pose.pose.position.z =
                (wp.pose.pose.position.z + lane.waypoints.at(wp_idx + 1).pose.pose.position.z) / 2;
            wp.twist.twist.linear.x =
                (wp.twist.twist.linear.x + lane.waypoints.at(wp_idx + 1).twist.twist.linear.x) / 2;

            ROS_INFO("Inserting stopline_interpolation_wp: #%d(%f, %f, %f)\n", wp_idx + 1, interpolation_point.x,
                     interpolation_point.y, interpolation_point.z);

            lane.waypoints.insert(lane.waypoints.begin() + wp_idx + 1, wp);
            wp_idx++;

            //  lane.waypoints.at(wp_idx).wpstate.stop_state = g_vmap.findByKey(Key<RoadSign>(stopline.signid)).type;
          }
        }
      }
    }

// MISSION COMPLETE FLAG
#define NUM_OF_SET_MISSION_COMPLETE_FLAG 3
    size_t wp_idx = lane.waypoints.size();
    int counter = 0;
    for (int counter = 0;
         counter <= (wp_idx <= NUM_OF_SET_MISSION_COMPLETE_FLAG ? wp_idx : NUM_OF_SET_MISSION_COMPLETE_FLAG); counter++)
    {
      lane.waypoints.at(--wp_idx).wpstate.event_state = autoware_msgs::WaypointState::TYPE_EVENT_GOAL;
    }
  }
}

// for based waypoint
void DecisionMakerNode::callbackFromLaneWaypoint(const autoware_msgs::LaneArray& msg)
{
  ROS_INFO("[%s]:LoadedWaypointLaneArray\n", __func__);

  current_status_.based_lane_array = msg;
  setEventFlag("received_based_lane_waypoint", true);
}

void DecisionMakerNode::callbackFromFinalWaypoint(const autoware_msgs::lane& msg)
{
  current_status_.finalwaypoints = msg;
  setEventFlag("received_finalwaypoints", true);

#if 0
  if (!ctx->isCurrentState(state_machine::DRIVE_STATE))
  {
    ROS_DEBUG("State is not DRIVE_STATE[%s]", ctx->getCurrentStateName().c_str());
    return;
  }

  // cached
  current_finalwaypoints_ = msg;
  if (current_finalwaypoints_.waypoints.empty())
  {
    return;
  }

  // stopline
  static size_t previous_idx = 0;

  size_t idx;
  idx = param_stopline_target_waypoint_ + (current_velocity_ * param_stopline_target_ratio_);
  idx = current_finalwaypoints_.waypoints.size() - 1 > idx ? idx : current_finalwaypoints_.waypoints.size() - 1;

  CurrentStoplineTarget_ = current_finalwaypoints_.waypoints.at(idx);
  try
  {
    for (size_t i = (previous_idx > idx) ? idx : previous_idx; i <= idx; i++)
    {
      if (i < current_finalwaypoints_.waypoints.size())
      {
        if (current_finalwaypoints_.waypoints.at(i).wpstate.stop_state == autoware_msgs::WaypointState::TYPE_STOPLINE)
        {
          ctx->setCurrentState(state_machine::DRIVE_ACC_STOPLINE_STATE);
          closest_stopline_waypoint_ = CurrentStoplineTarget_.gid;
        }
        if (current_finalwaypoints_.waypoints.at(i).wpstate.stop_state == autoware_msgs::WaypointState::TYPE_STOP)
        {
          ctx->setCurrentState(state_machine::DRIVE_ACC_STOP_STATE);
          closest_stop_waypoint_ = CurrentStoplineTarget_.gid;
        }
      }
    }
  }
  catch (std::out_of_range &oor)
  {
    fprintf(stderr, "[%s:%d]: access=%d, size=%d\n", __FILE__, __LINE__, idx, current_finalwaypoints_.waypoints.size());
  }
  previous_idx = idx;
// steering
#if 0
  idx = current_finalwaypoints_.waypoints.size() - 1 > param_target_waypoint_ ?
            param_target_waypoint_ :
            current_finalwaypoints_.waypoints.size() - 1;
#endif
  double _distance = 0.0;
  double _distance_threshold = 30;  // 30[m] is decided by japanese law.

  //
  amathutils::point _prev_point(current_finalwaypoints_.waypoints.at(0).pose.pose.position.x,
                                current_finalwaypoints_.waypoints.at(0).pose.pose.position.y,
                                current_finalwaypoints_.waypoints.at(0).pose.pose.position.z);

  idx = 0;
  for (auto &wp : current_finalwaypoints_.waypoints)
  {
    amathutils::point _next_point(wp.pose.pose.position.x, wp.pose.pose.position.y, wp.pose.pose.position.z);

    _distance += amathutils::find_distance(&_prev_point, &_next_point);
    idx++;
    if (_distance >= _distance_threshold)
    {
      break;
    }
    if (idx >= current_finalwaypoints_.waypoints.size())
    {
      idx -= 1;
      break;
    }
    _prev_point = _next_point;
  }

  if (ctx->isCurrentState(state_machine::DRIVE_BEHAVIOR_LANECHANGE_LEFT_STATE))
  {
    ctx->setCurrentState(state_machine::DRIVE_STR_LEFT_STATE);
  }
  if (ctx->isCurrentState(state_machine::DRIVE_BEHAVIOR_LANECHANGE_RIGHT_STATE))
  {
    ctx->setCurrentState(state_machine::DRIVE_STR_RIGHT_STATE);
  }
  else
  {
    idx = (idx >= current_finalwaypoints_.waypoints.size()) ? idx = current_finalwaypoints_.waypoints.size() - 1 : idx;
    try
    {
      state_machine::StateFlags _TargetStateFlag;
      for (size_t i = idx; i > 0; i--)
      {
        _TargetStateFlag = getStateFlags(current_finalwaypoints_.waypoints.at(i).wpstate.steering_state);
        if (_TargetStateFlag != state_machine::DRIVE_STR_STRAIGHT_STATE)
        {
          break;
        }
      }
      ctx->setCurrentState(_TargetStateFlag);
    }
    catch (std::out_of_range &oor)
    {
      fprintf(stderr, "[%s:%d]: access=%d, size=%d\n", __FILE__, __LINE__, idx,
              current_finalwaypoints_.waypoints.size());
    }
  }
  // for publish plan of velocity
  publishToVelocityArray();
#endif
}
void DecisionMakerNode::callbackFromTwistCmd(const geometry_msgs::TwistStamped& msg)
{
}

void DecisionMakerNode::callbackFromClosestWaypoint(const std_msgs::Int32& msg)
{
  current_status_.closest_waypoint = msg.data;
}

void DecisionMakerNode::callbackFromCurrentPose(const geometry_msgs::PoseStamped& msg)
{
  current_status_.pose = msg.pose;
}

void DecisionMakerNode::callbackFromCurrentVelocity(const geometry_msgs::TwistStamped& msg)
{
  current_status_.velocity = amathutils::mps2kmph(msg.twist.linear.x);
}
}
