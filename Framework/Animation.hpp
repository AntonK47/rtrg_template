#pragma once

#include <vector>

#include "Core.hpp"
#include "Math.hpp"


namespace Framework
{
	namespace Animation
	{


		struct AnimationData
		{
			U32 offset;
			U32 count;
			U32 frames;
			Float duration;
			std::string animationName;
		};

		struct AnimationInstance
		{
			AnimationData data;
			Float playbackRate;
			Float startTime;
			bool loop;
		};


		struct JointAnimationData
		{
			Math::Quaternion rotation;
			Math::Vector3 translation;
		};

		struct LocalPose
		{
			std::vector<JointAnimationData> data;
		};

		struct Joint
		{
			Math::Matrix4x4 inverseBindPose;
			I32 parentIndex;
			std::string name; // For debug only
		};
		struct Skeleton
		{
			std::vector<Joint> joints;
		};

		struct AnimationDataSet
		{
			std::vector<AnimationData> animations;
			std::vector<JointAnimationData> animationDatabase;
		};


		namespace Detail
		{
			inline Math::Matrix4x4 ComputeJointMatrix(const JointAnimationData& joint)
			{
				const auto rotor = Math::Matrix4x4::From(joint.rotation);
				const auto translate = Math::Matrix4x4::TranslationFrom(joint.translation);
				return translate * rotor;
			}
		} // namespace Detail

		inline std::vector<Math::Matrix4x4> ComputeJointsMatrices(const LocalPose& pose, const Skeleton& skeleton)
		{
			assert(pose.data.size() > 0);
			assert(pose.data.size() == skeleton.joints.size());

			const auto totalJoints = pose.data.size();

			auto matrices = std::vector<Math::Matrix4x4>{};
			matrices.resize(totalJoints);

			const auto rootIndex = 0;
			matrices[rootIndex] = Detail::ComputeJointMatrix(pose.data[rootIndex]);

			for (auto i = 1; i < matrices.size(); i++)
			{
				matrices[i] = matrices[skeleton.joints[i].parentIndex] * Detail::ComputeJointMatrix(pose.data[i]);
			}

			return matrices;
		}

		inline void ApplyBindPose(std::vector<Math::Matrix4x4>& jointsMatrices, const Skeleton& skeleton)
		{
			assert(jointsMatrices.size() > 0);
			assert(jointsMatrices.size() == skeleton.joints.size());

			for (auto i = 1; i < jointsMatrices.size(); i++)
			{
				jointsMatrices[i] = jointsMatrices[skeleton.joints[i].parentIndex] * jointsMatrices[i];
			}
		}

		inline LocalPose SamplePose(const AnimationDataSet& animationDataSet, const AnimationData& data, Float time)
		{
			auto pose = LocalPose{}; // normally this should be allocated outside of the function call
			pose.data.resize(data.count);

			float fps = data.frames / data.duration;
			float timePerFrame = data.duration / data.frames;
			float index = time * fps;


			const auto first = Math::Modulo(As<U32>(Math::Floor(index)), data.frames);
			auto second = Math::Modulo(As<U32>(Math::Ceil(index)), data.frames);


			float rest = time - first * timePerFrame;


			if (first == second)
			{
				for (auto i = 0; i < data.count; i++)
				{
					const auto& joint = animationDataSet.animationDatabase[data.offset + first * data.count + i];
					pose.data[i] = joint;
				}
			}
			else
			{
				for (auto i = 0; i < data.count; i++)
				{
					const auto& jointA = animationDataSet.animationDatabase[data.offset + first * data.count + i];
					const auto& jointB = animationDataSet.animationDatabase[data.offset + second * data.count + i];

					pose.data[i].rotation = Math::Slerp(jointA.rotation, jointB.rotation, rest);
					pose.data[i].translation = Math::Mix(jointA.translation, jointB.translation, rest);
				}
			}

			return pose;
		}

		inline LocalPose SamplePose(const AnimationDataSet& animationDataSet, const AnimationInstance& instance,
									Float globalTime)
		{
			Float localTime = (globalTime - instance.startTime) * instance.playbackRate;

			if (instance.loop)
			{
				localTime = Math::Modulo(localTime, instance.data.duration);
			}

			return SamplePose(animationDataSet, instance.data, localTime);
		}

	} // namespace Animation
} // namespace Framework