#ifndef _TEMPLATE_MATCHING_DEFINE_H 
#define _TEMPLATE_MATCHING_DEFINE_H 

#pragma once

#ifdef _WIN32
#pragma warning(disable: 4251)
#endif  // _WIN32

#ifdef LIB_SHARED_BUILD
#ifdef _WIN32
#ifdef LIB_EXPORTS
#define LIB_API __declspec(dllexport)
#else
#define LIB_API __declspec(dllimport)
#endif  // MY_LIB_EXPORTS
#else
#define LIB_API
#endif  // _WIN32
#else
#define LIB_API
#endif  // MY_LIB_SHARED_BUILD


#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>   /* imread/imwrite/imencode/imdecode (OpenCV1 时代在 highgui) */

// ============================================================
// OpenCV 4 兼容层 / OpenCV 4 compatibility shim
// 原 opencv_matching 面向 OpenCV 1.x 的 C 风格宏（CV_TM_CCORR / CV_FILLED /
// CV_GRAY2BGR / cvNamedWindow）在 OpenCV 4 中已迁移到 cv:: 命名空间。
// 首次编译 OpenCV 4 时在此补齐，便于同一份源码跨版本构建。
// ============================================================
#ifndef TEMPLATE_MATCHING_OPENCV4_SHIM
#define TEMPLATE_MATCHING_OPENCV4_SHIM
#include <cstdio>
#include <iostream>

#ifndef CV_TM_SQDIFF
#define CV_TM_SQDIFF          cv::TM_SQDIFF
#define CV_TM_SQDIFF_NORMED   cv::TM_SQDIFF_NORMED
#define CV_TM_CCORR           cv::TM_CCORR
#define CV_TM_CCORR_NORMED    cv::TM_CCORR_NORMED
#define CV_TM_CCOEFF          cv::TM_CCOEFF
#define CV_TM_CCOEFF_NORMED   cv::TM_CCOEFF_NORMED
#endif
#ifndef CV_FILLED
#define CV_FILLED             cv::FILLED
#endif
#ifndef CV_GRAY2BGR
#define CV_GRAY2BGR           cv::COLOR_GRAY2BGR
#endif
#ifndef CV_BGR2GRAY
#define CV_BGR2GRAY           cv::COLOR_BGR2GRAY
#endif
// HighGUI：cvNamedWindow() 在 OpenCV 4 已移除，统一改名
#define cvNamedWindow         cv::namedWindow
using std::cout;
using std::endl;
#endif // TEMPLATE_MATCHING_OPENCV4_SHIM

namespace template_matching
{
	// 匹配器类型
	enum LIB_API MatcherType { 
		PATTERN = 0,

	};

	// 匹配器参数
	struct LIB_API MatcherParam
	{
		// 匹配器类型
		MatcherType matcherType = MatcherType::PATTERN;

		// 最大匹配目标数量
		int maxCount = 200;

		// 匹配得分阈值
		double scoreThreshold = 0.5;

		// 重叠框去重iou阈值
		double iouThreshold = 0.0;

		// 匹配角度范围
		double angle = 0;

		// 顶层金字塔最小面积
		double minArea = 256;

	};
	
	// 匹配结果
	struct LIB_API MatchResult
	{
		cv::Point2d LeftTop; // 左上角点
		cv::Point2d LeftBottom; // 左下角点
		cv::Point2d RightTop; // 右上角点
		cv::Point2d RightBottom; // 右下角点
		cv::Point2d Center; // 中心点

		double Angle; // 角度
		double Score; // 匹配得分
	};

}

#endif
