#include <math.h>
#include "livefacereco.hpp"
#include <time.h>
#include "math.hpp"
#include "parallel_video_capture.hpp"
#include "mtcnn_new.h"
#include "FacePreprocess.h"
#include <algorithm>
#include "path.h"

#include <iostream>
#include <fstream>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

std::map<std::string,cv::Mat> face_descriptors_dict;


std::vector<std::string> split(const std::string& s, char seperator)
{
   std::vector<std::string> output;

    std::string::size_type prev_pos = 0, pos = 0;

    while((pos = s.find(seperator, pos)) != std::string::npos)
    {
        std::string substring( s.substr(prev_pos, pos-prev_pos) );

        output.push_back(substring);

        prev_pos = ++pos;
    }

    output.push_back(s.substr(prev_pos, pos-prev_pos)); // Last word

    return output;
}

Bbox  getLargestBboxFromBboxVec(const std::vector<Bbox> & faces_info)
{
    if(faces_info.size()>0)
    {
        int lagerest_face=0,largest_number=0;
        for (int i = 0; i < faces_info.size(); i++){
            int y_ = (int) faces_info[i].y2 * ratio_y;
            int h_ = (int) faces_info[i].y1 * ratio_y;
            if (h_-y_> lagerest_face){
                lagerest_face=h_-y_;
                largest_number=i;                   
            }
        }
        
        return faces_info[largest_number];
    }
    return Bbox();
}

cv::Mat createFaceLandmarkMatrixfromBBox(const Bbox  & box)
{
    float v2[5][2] =
                {{box.ppoint[0], box.ppoint[5]},
                {box.ppoint[1], box.ppoint[6]},
                {box.ppoint[2], box.ppoint[7]},
                {box.ppoint[3], box.ppoint[8]},
                {box.ppoint[4], box.ppoint[9]},
                };
    cv::Mat dst(5, 2, CV_32FC1, v2);
    memcpy(dst.data, v2, 2 * 5 * sizeof(float));

    return dst.clone();
}

cv::Mat alignFaceImage(const cv::Mat & frame, const Bbox & bbox,const cv::Mat & gt_landmark_matrix)
{
    cv::Mat face_landmark = createFaceLandmarkMatrixfromBBox(bbox);

    cv::Mat transf = FacePreprocess::similarTransform(face_landmark, gt_landmark_matrix);

    cv::Mat aligned = frame.clone();
    cv::warpPerspective(frame, aligned, transf, cv::Size(96, 112), INTER_LINEAR);
    resize(aligned, aligned, Size(112, 112), 0, 0, INTER_LINEAR);
     
    return aligned.clone();
}

cv::Mat createFaceLandmarkGTMatrix()
{
    // groundtruth face landmark
    float v1[5][2] = {
            {30.2946f, 51.6963f},
            {65.5318f, 51.5014f},
            {48.0252f, 71.7366f},
            {33.5493f, 92.3655f},
            {62.7299f, 92.2041f}};

    cv::Mat src(5, 2, CV_32FC1, v1); 
    memcpy(src.data, v1, 2 * 5 * sizeof(float));
    return src.clone();
}

void calculateFaceDescriptorsFromDisk(Arcface & facereco,std::map<std::string,cv::Mat> & face_descriptors_map)
{
    const std::string project_path = images_path;
    std::string pattern_jpg = project_path + "/*.jpg";
	std::vector<cv::String> image_names;
    
	cv::glob(pattern_jpg, image_names);
    
    int image_number=image_names.size();

	if (image_number == 0) {
		std::cout << "No image files[jpg]" << std::endl;
        std::cout << "At least one image of 112*112 should be put into the img folder. Otherwise, the program will broke down." << std::endl;
        exit(0);
	}
    //cout <<"loading pictures..."<<endl;
    //cout <<"image number in total:"<<image_number<<endl;
    cv::Mat  face_img;
    unsigned int img_idx = 0;
    cv::Mat face_landmark_gt_matrix = createFaceLandmarkGTMatrix();
    cv::Mat face_descriptor;
    cv::Mat aligned_faceimg;
    std::vector<Bbox> faces_info;
  
    //convert to vector and store into fc, whcih is benefical to furthur operation
	for(auto const & img_name:image_names)
    {
        //cout <<"image name:"<<img_name<<endl;
        auto splitted_string = split(img_name,'/');
        auto splitted_string_2 = splitted_string[splitted_string.size()-1];
        std::size_t name_length = splitted_string_2.find_last_of('.');
        auto person_name =  splitted_string_2.substr(0,name_length);
		
		
        face_img = cv::imread(img_name);
		faces_info = detect_mtcnn(face_img); 

        if(faces_info.size() >=1){
        auto large_box = getLargestBboxFromBboxVec(faces_info);
		aligned_faceimg = alignFaceImage(face_img,large_box,face_landmark_gt_matrix);
        face_descriptor = facereco.getFeature(aligned_faceimg);
        

        face_descriptors_map[person_name] = Statistics::zScore(face_descriptor);
        }
    }
        // cv::imwrite("./read_img.jpg",face_img);
        // cv::imwrite("./face_img.jpg",aligned_faceimg);
        // cv::imwrite("./landmark.jpg",face_descriptor);

    cout <<"loading succeed! "<<image_number<<" pictures in total"<<endl;
    
}
void calculateFaceDescriptorsFromImgDataset(Arcface & facereco,std::map<std::string,std::list<cv::Mat>> & img_dataset,std::map<std::string, std::list<cv::Mat>> & face_descriptors_map)
{
    int img_idx = 0;
    const int image_number = img_dataset.size()*5;
    for(const auto & dataset_pair:img_dataset)
    {
        const std::string person_name = dataset_pair.first;

        std::list<cv::Mat> descriptors;
        if (image_number == 0) {
            cout << "No image files[jpg]" << endl;
            return;
        }
        else{
            cout <<"loading pictures..."<<endl;
            for(const auto & face_img:dataset_pair.second)
            {
                cv::Mat face_descriptor = facereco.getFeature(face_img);
                descriptors.push_back( Statistics::zScore(face_descriptor));
                cout << "now loading image " << ++img_idx << " out of " << image_number << endl;
                //printf("\rloading[%.2lf%%]\n",  (++img_idx)*100.0 / (image_number));
            }
            face_descriptors_map[person_name] = std::move(descriptors);
        }
        
    }
}
void loadLiveModel( Live & live )
{
    //Live detection configs
    struct ModelConfig config1 ={2.7f,0.0f,0.0f,80,80,"model_1",false};
    struct ModelConfig config2 ={4.0f,0.0f,0.0f,80,80,"model_2",false};
    vector<struct ModelConfig> configs;
    configs.emplace_back(config1);
    configs.emplace_back(config2);
    live.LoadModel(configs);
}

LiveFaceBox Bbox2LiveFaceBox(const Bbox  & box)
{
    float x_   =  box.x1;
    float y_   =  box.y1;
    float x2_ =  box.x2;
    float y2_ =  box.y2;
    int x = (int) x_ ;
    int y = (int) y_;
    int x2 = (int) x2_;
    int y2 = (int) y2_;
    struct LiveFaceBox  live_box={x_,y_,x2_,y2_} ;
    return live_box;
}



std::string  getClosestFaceDescriptorPersonName(std::map<std::string,cv::Mat> & disk_face_descriptors, cv::Mat face_descriptor)
{
    vector<double> score_(disk_face_descriptors.size());

    std::vector<std::string> labels;

    int i = 0;

    for(const auto & disk_descp:disk_face_descriptors)
    {
        // cout << "comparing with " << disk_descp.first << endl;

        score_[i] = (Statistics::cosineDistance(disk_descp.second, face_descriptor));
        //cout << "score  " << score_[i] << endl;
        labels.push_back(disk_descp.first);
        i++;
    }
    int maxPosition = max_element(score_.begin(),score_.end()) - score_.begin(); 
    int pos = score_[maxPosition]>face_thre?maxPosition:-1;
    //cout << "score_[maxPosition] " << score_[maxPosition] << endl;
    std::string person_name = "";
    if(pos>=0)
    {
        person_name = labels[pos];
    }
    score_.clear();

    return person_name;
}
std::string  getClosestFaceDescriptorPersonName(std::map<std::string,std::list<cv::Mat>> & disk_face_descriptors, cv::Mat face_descriptor)
{
    vector<std::list<double>> score_(disk_face_descriptors.size());

    std::vector<std::string> labels;

    int i = 0;

    for(const auto & disk_descp:disk_face_descriptors)
    {
        for(const auto & descriptor:disk_descp.second)
        {
            score_[i].push_back(Statistics::cosineDistance(descriptor, face_descriptor));
        }

        labels.push_back(disk_descp.first);
        i++;
    
    }

    int maxPosition = max_element(score_.begin(),score_.end()) - score_.begin();
    
    auto get_max_from_score_list = 
                            [&]()
                            {
                                double max = *score_[maxPosition].begin();
                                for(const auto & elem:score_[maxPosition])
                                {
                                    if(max<elem)
                                    {
                                        max = elem;
                                    }
                                }
                                return max;
                            }; 

    double max = get_max_from_score_list();

    int pos = max>face_thre?maxPosition:-1;

    std::string person_name = "";
    if(pos>=0)
    {
        person_name = labels[pos];
    }

    score_.clear();

    return person_name;
}

void add_person_funcation(Arcface & facereco){

    const std::string addpic_path = addimg_path;
    std::string pattern_jpg = addpic_path + "/*.jpg";
	std::vector<cv::String> image_names;
    
	cv::glob(pattern_jpg, image_names);   
    int image_number=image_names.size();

	if (image_number != 0) {   
        cv::Mat  face_img;
        unsigned int img_idx = 0;
        cv::Mat face_landmark_gt_matrix = createFaceLandmarkGTMatrix();
        cv::Mat face_descriptor;
        cv::Mat aligned_faceimg;
        std::vector<Bbox> faces_info;

       for(auto const & img_name:image_names){
            auto splitted_string = split(img_name,'/');
            auto splitted_string_2 = splitted_string[splitted_string.size()-1];
            std::size_t name_length = splitted_string_2.find_last_of('.');
            auto person_name =  splitted_string_2.substr(0,name_length);

            face_img = cv::imread(img_name);
            faces_info = detect_mtcnn(face_img); 

            if(faces_info.size() >=1){

                std::string copy_path = images_path + img_name;
                cv::imwrite(copy_path,face_img);                             //下次重启自动取数据库里面拿数据 

                auto large_box = getLargestBboxFromBboxVec(faces_info);
                aligned_faceimg = alignFaceImage(face_img,large_box,face_landmark_gt_matrix);
                face_descriptor = facereco.getFeature(aligned_faceimg);
                
                //face_descriptors_map[person_name] = Statistics::zScore(face_descriptor);
                face_descriptors_dict[person_name] = face_descriptor;
                std::string local_path = addpic_path + img_name;
                fs::remove(local_path);                                     //拷贝一张删除一张
        }
       }                
        
	}
}

void del_person_funcation(string name){                                      //需要重启生效

    if(name.size() != 0)
    {
        std::string project_path = images_path + name + ".jpg";
        fs::remove(project_path); 
    }
}

void NV21ToMat(const uint8_t* nv21_data, int width, int height, cv::Mat& output_mat) {
    // 为YUV数据分配内存
    uint8_t* yuv_data = new uint8_t[width * height * 3 / 2];
 
    // Y分量和VU分量
    uint8_t* y_plane = yuv_data;
    uint8_t* v_u_plane = yuv_data + width * height;
 
    // 拷贝Y分量和UV分量
    memcpy(y_plane, nv21_data, width * height);
    for (int i = 0; i < height / 2; ++i) {
        memcpy(v_u_plane + 2 * i * width, nv21_data + height * width + 2 * i * (width / 2), width);
    }
 
    // 创建cv::Mat，并设置正确的数据指针和步长
    output_mat = cv::Mat(height + height / 2, width, CV_8UC1, yuv_data);
 
    // 转换成RGB
    cv::cvtColor(output_mat, output_mat, cv::COLOR_YUV2BGR_NV21);
 
    // 释放内存
    delete[] yuv_data;
}

int MTCNNDetection()
{
    Arcface facereco;
    string del_name;
    calculateFaceDescriptorsFromDisk(facereco,face_descriptors_dict);

    Live live;
    loadLiveModel(live);


    //ParallelVideoCapture cap("udpsrc port=5000 ! application/x-rtp, payload=96 ! rtpjitterbuffer ! rtph264depay ! avdec_h264 ! videoconvert ! appsink sync=false",cv::CAP_GSTREAMER,30); //using camera capturing
    //ParallelVideoCapture cap("udpsrc device=/dev/video0 ! application/x-rtp, payload=96 ! rtpjitterbuffer ! rtph264depay ! avdec_h264 ! videoconvert ! appsink sync=false",cv::CAP_GSTREAMER,30); //using camera capturing
    //ParallelVideoCapture cap("/home/pi/testvideo.mp4");

    //ParallelVideoCapture cap(0);    // USB 摄像头设备号    android not used ,linux please open
    //cap.startCapture();             // 异步获取帧图像      android not used,linux please open

    //std::cout<<"okay!\n";

    // if (!cap.isOpened()) {                               //android not used,linux please open
    //     cerr << "cannot get image" << endl;              //android not used,linux please open
    //     return -1;                                       //android not used,linux please open
    // }                                                    //android not used,linux please open

    float confidence;
    Mat frame;                 //图像帧   
    cv::Mat face_landmark_gt_matrix = createFaceLandmarkGTMatrix();
    std::string liveface;
    float ratio_x = 1;
    float ratio_y = 1;

    //NV21ToMat(nv21_data, width, height, frame);           //给出nv21的数据，宽和高        linux not used,android please open

    while(!frame.empty())
    {
        //frame = cap.getFrame();     //得到帧图像，安卓环境需要把nv21转成BGRA格式               android not used,linux please open
        if(frame.empty())
        {
            continue;
        } 
        //detect faces
        std::vector<Bbox> faces_info = detect_mtcnn(frame);      //人脸检测
        if(faces_info.size()>=1)
        {
            auto large_box = getLargestBboxFromBboxVec(faces_info);
            //cout << "large_box got" << endl;
            LiveFaceBox live_face_box = Bbox2LiveFaceBox(large_box);
            
            cv::Mat aligned_img = alignFaceImage(frame,large_box,face_landmark_gt_matrix);
            //cout << "aligned_img got" << endl;
            cv::Mat face_descriptor = facereco.getFeature(aligned_img);
            // normalize
            face_descriptor = Statistics::zScore(face_descriptor);
            //cout << "face_descriptor created" << endl;
            std::string person_name = getClosestFaceDescriptorPersonName(face_descriptors_dict,face_descriptor);    //对比识别

            add_person_funcation(facereco);                  // 添加新人脸，图片丢add_picture文件夹
            del_person_funcation(del_name);                  // 删除人脸数据，需重启生效
            

            confidence = live.Detect(frame,live_face_box);        // 活体检测，不是特别精准，按照需要自行添加，不要也可以毙掉

            if(!person_name.empty() && confidence >= true_thre)                                                  // 活体人脸
            {               
                //putText(frame, person_name, cv::Point(15, 80), cv::FONT_HERSHEY_SIMPLEX,0.75, cv::Scalar(255, 255, 0),2);                                                // 打印人名 person_name
                cout<<person_name<<endl;
                cv::rectangle(frame, Point(large_box.x1*ratio_x, large_box.y1*ratio_y), Point(large_box.x2*ratio_x,large_box.y2*ratio_y), cv::Scalar(0, 0, 255), 2);    // 框出人脸，矩形框，左上角和右下角的坐标位置
                
            }        
        }
    //NV21ToMat(nv21_data, width, height, frame);           //给出nv21的数据，宽和高        linux not used,android please open         更新数据

	 //cv::imshow("test",frame);
         //char keyvalue = cv::waitKey(1);
    
	 //if (keyvalue == 113||keyvalue == 81)
         //    break;
     }
    //cap.stopCapture();             // android not used,linux please open
    return 0;
}


