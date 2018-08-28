#include <mocap_optitrack/rigid_body_publisher.h>

#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Pose2D.h>

namespace mocap_optitrack
{

namespace utilities
{
  geometry_msgs::PoseStamped getRosPose(RigidBody const& body, bool newCoordinates)
  {
    geometry_msgs::PoseStamped poseStampedMsg;
    if (newCoordinates)
    {
      // Motive 1.7+ coordinate system
      poseStampedMsg.pose.position.x = -body.pose.position.x;
      poseStampedMsg.pose.position.y = body.pose.position.z;
      poseStampedMsg.pose.position.z = body.pose.position.y;
  
      poseStampedMsg.pose.orientation.x = -body.pose.orientation.x;
      poseStampedMsg.pose.orientation.y = body.pose.orientation.z;
      poseStampedMsg.pose.orientation.z = body.pose.orientation.y;
      poseStampedMsg.pose.orientation.w = body.pose.orientation.w;
    }
    else
    {
      // y & z axes are swapped in the Optitrack coordinate system
      poseStampedMsg.pose.position.x = body.pose.position.x;
      poseStampedMsg.pose.position.y = -body.pose.position.z;
      poseStampedMsg.pose.position.z = body.pose.position.y;
  
      poseStampedMsg.pose.orientation.x = body.pose.orientation.x;
      poseStampedMsg.pose.orientation.y = -body.pose.orientation.z;
      poseStampedMsg.pose.orientation.z = body.pose.orientation.y;
      poseStampedMsg.pose.orientation.w = body.pose.orientation.w;
    }
    return poseStampedMsg;
  }   
}

RigidBodyPublisher::RigidBodyPublisher(ros::NodeHandle &nh, 
  Version const& natNetVersion,
  PublisherConfiguration const& config) :
    config(config)
{
  if (config.publishPose)
    posePublisher = nh.advertise<geometry_msgs::PoseStamped>(config.poseTopicName, 1000);

  if (config.publishPose2d)
    pose2dPublisher = nh.advertise<geometry_msgs::Pose2D>(config.pose2dTopicName, 1000);

  // Motive 1.7+ uses a new coordinate system
  useNewCoordinates = (natNetVersion >= Version("1.7"));
}

RigidBodyPublisher::~RigidBodyPublisher()
{

}

void RigidBodyPublisher::publish(ros::Time const& time, RigidBody const& body)
{
  // don't do anything if no new data was provided
  if (!body.hasValidData())
  {
    return;
  }

  // NaN?
  if (body.pose.position.x != body.pose.position.x)
  {
    return;
  }

  geometry_msgs::PoseStamped pose = utilities::getRosPose(body, useNewCoordinates);
  pose.header.stamp = time;

  if (config.publishPose)
  {
    pose.header.frame_id = config.parentFrameId;
    posePublisher.publish(pose);
  }

  tf::Quaternion q(pose.pose.orientation.x,
                   pose.pose.orientation.y,
                   pose.pose.orientation.z,
                   pose.pose.orientation.w);

  // publish 2D pose
  if (config.publishPose2d)
  {
    geometry_msgs::Pose2D pose2d;
    pose2d.x = pose.pose.position.x;
    pose2d.y = pose.pose.position.y;
    pose2d.theta = tf::getYaw(q);
    pose2dPublisher.publish(pose2d);
  }

  if (config.publishTf)
  {
    // publish transform
    tf::Transform transform;
    transform.setOrigin( tf::Vector3(pose.pose.position.x,
                                     pose.pose.position.y,
                                     pose.pose.position.z));

    // Handle different coordinate systems (Arena vs. rviz)
    transform.setRotation(q);
    tfPublisher.sendTransform(tf::StampedTransform(transform, 
      time, 
      config.parentFrameId, 
      config.childFrameId));
  }
}


RigidBodyPublishDispatcher::RigidBodyPublishDispatcher(
  ros::NodeHandle &nh, 
  Version const& natNetVersion,
  PublisherConfigurations const& configs)
{
  for (auto const& config : configs)
  {
    rigidBodyPublisherMap[config.rigidBodyId] = 
      RigidBodyPublisherPtr(new RigidBodyPublisher(nh, natNetVersion, config));
  }
}

void RigidBodyPublishDispatcher::publish(
  ros::Time const& time, 
  std::vector<RigidBody> const& rigidBodies)
{
  for (auto const& rigidBody : rigidBodies)
  {
    auto const& iter = rigidBodyPublisherMap.find(rigidBody.bodyId);

    if (iter != rigidBodyPublisherMap.end())
    {
      (*iter->second).publish(time, rigidBody);
    }
  }
}


} // namespace