#ifndef HEADPOSEDETECTOR_H
#define HEADPOSEDETECTOR_H
#include <opencv2/opencv.hpp>
#include <vector>
#include <Eigen/Eigen>
#include <string>
#include <thread>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/shape_predictor.h>
#include <opencv2/core/eigen.hpp>
#include <fstream>
#include <QDebug>
#include <QtNetwork>
#include <FlightAgxSettings.h>
#include <queue>
#include <mutex>
#include <opencv2/tracking/tracker.hpp>
#include <fagx_datatype.h>
#include <KalmanFilter.h>

class FaceDetector {
    dlib::frontal_face_detector detector = dlib::get_frontal_face_detector();
public:
    FaceDetector() {

    }
    virtual ~FaceDetector() {

    }

    virtual cv::Rect2d detect(cv::Mat frame, cv::Rect2d predict_roi);
};

class LandmarkDetector {
    dlib::shape_predictor predictor;
public:
    LandmarkDetector(std::string model_path) {
        dlib::deserialize(model_path.c_str()) >> predictor;
    }

    virtual ~LandmarkDetector() {}

    virtual CvPts detect(cv::Mat frame, cv::Rect roi);
};

class HeadPoseDetector;

class HeadPoseTrackDetectWorker: public QObject {
    Q_OBJECT

    HeadPoseDetector * hd;
    bool is_running = false;
public slots:
    void run();
    void stop();

public:
    HeadPoseTrackDetectWorker(HeadPoseDetector * _hd): hd(_hd) {}
};

class HeadPoseDetector: public QObject {
    Q_OBJECT

    FaceDetector * fd = nullptr;
    LandmarkDetector * lmd = nullptr;

    ExtendKalmanFilter12DOF ekf;
    bool is_running = false;
    cv::VideoCapture cap;

    std::mutex detect_mtx;

    std::mutex detect_frame_mtx;

    bool frame_pending_detect = false;
    cv::Mat frame_need_to_detect;
    cv::Rect2d roi_need_to_detect;

    std::thread detect_thread;

    QThread mainThread;
    QTimer * main_loop_timer;
    QThread detectThread;

    std::pair<bool, Pose> solve_face_pose(CvPts landmarks, cv::Mat & frame);

    cv::Mat rvec_init, tvec_init;
    std::vector<cv::Point3f> model_points_68;

    Eigen::Matrix3d Rface, Rcam;

    bool first_solve_pose = true;
    Eigen::Vector3d Tinit;

    std::vector<cv::Mat> frames;

    cv::Rect2d last_roi;
    int frame_count = 0;

    cv::Ptr<cv::Tracker> tracker;

    cv::Mat preview_image;

    cv::Mat last_clean_frame;
    std::vector<int> last_ids;

    CvPts last_landmark_pts;

    double t0;
    double dt = 0.03;
    double last_t = 0;
public:


    HeadPoseDetector() {
        is_running = false;
        fd = new FaceDetector;
        lmd = new LandmarkDetector(settings->landmark_model);

        rvec_init = (cv::Mat_<double>(3,1) << 0.0, 0.0, -3.14392813);
        tvec_init = (cv::Mat_<double>(3,1) << 0.0, 0.0, -500);

        Rface << 0,  1, 0,
                    0,  0, -1, 
                    -1, 0, 0;

        Rcam << 0, 0, -1,
                -1, 0, 0,
                 0, 1, 0;

        std::ifstream model_file (settings->model);
        if (model_file.is_open())
        {
            double px, py, pz;
            while (!model_file.eof())
            {
                model_file >> px >> py >> pz;
                model_points_68.push_back(cv::Point3d(px, py, - pz));
            }
        }
        model_file.close();

        if(model_points_68.size() != 68) {
            qDebug() << "Model file error!";
            exit(-1);
        }


        connect(this, SIGNAL(start()),
                this, SLOT(start_slot()));

        connect(this, SIGNAL(stop()),
                this, SLOT(stop_slot()));
        this->moveToThread(&mainThread);
        mainThread.start();
    }

    std::pair<bool, Pose> detect_head_pose(cv::Mat & frame, double t, double dt);

    void run_thread();

    void run_detect_thread();
    void reset();

public:
    cv::Mat & get_preview_image() {
        return preview_image;
    }

signals:
    void start();
    void stop();

    void on_detect_pose(double t, Pose pose);
    void on_detect_pose6d(double t, Pose6DoF pose);
    void on_detect_pose6d_raw(double t, Pose6DoF pose);
    void on_detect_twist(double t, Eigen::Vector3d w, Eigen::Vector3d v);
    void on_detect_P(double t, Matrix13d P);

private slots:
    void loop();
    void start_slot();
    void stop_slot();

};

void reduceVector(std::vector<cv::Point2f> &v, std::vector<uchar> status);
void reduceVector(std::vector<int> &v, std::vector<uchar> status);
#endif // HEADPOSEDETECTOR_H
