// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2019 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Han Wang, Asher Elmquist
// =============================================================================
//
// =============================================================================

#include "chrono_sensor/filters/ChFilterRadarProcess.h"
#include "chrono_sensor/utils/CudaMallocHelper.h"
#include "chrono_sensor/cuda/radarprocess.cuh"
#include "chrono_sensor/utils/Dbscan.h"

namespace chrono {
namespace sensor {

ChFilterRadarProcess::ChFilterRadarProcess(std::string name) : ChFilter(name) {}

CH_SENSOR_API void ChFilterRadarProcess::Initialize(std::shared_ptr<ChSensor> pSensor,
                                                    std::shared_ptr<SensorBuffer>& bufferInOut) {
    if (!bufferInOut)
        InvalidFilterGraphNullBuffer(pSensor);

    m_buffer_in = std::dynamic_pointer_cast<SensorDeviceRadarBuffer>(bufferInOut);
    if (!m_buffer_in)
        InvalidFilterGraphBufferTypeMismatch(pSensor);

    // The sensor must be a radar
    if (auto pRadar = std::dynamic_pointer_cast<ChRadarSensor>(pSensor)) {
        m_cuda_stream = pRadar->GetCudaStream();
        m_hFOV = pRadar->GetHFOV();
        m_max_vert_angle = pRadar->GetMaxVertAngle();
        m_min_vert_angle = pRadar->GetMinVertAngle();
    } else {
        InvalidFilterGraphSensorTypeMismatch(pSensor);
    }
    m_radar = std::dynamic_pointer_cast<ChRadarSensor>(pSensor);
    m_buffer_out = chrono_types::make_shared<SensorDeviceProcessedRadarBuffer>();
    std::shared_ptr<RadarTrack[]> b(
        cudaHostMallocHelper<RadarTrack>(m_buffer_in->Width * m_buffer_in->Height),
        cudaHostFreeHelper<RadarTrack>);
    m_buffer_out->Buffer = std::move(b);
    m_buffer_out->Width = bufferInOut->Width;
    m_buffer_out->Height = bufferInOut->Height;
    bufferInOut = m_buffer_out;
}
CH_SENSOR_API void ChFilterRadarProcess::Apply() {
    cuda_radar_pointcloud_from_angles(m_buffer_in->Buffer.get(), m_buffer_out->Buffer.get(), (int)m_buffer_in->Width,
                                     (int)m_buffer_in->Height, m_hFOV, m_max_vert_angle, m_min_vert_angle,
                                     m_cuda_stream);

    auto buf = std::vector<RadarTrack>(m_buffer_out->Width * m_buffer_out->Height);
    cudaMemcpyAsync(buf.data(), m_buffer_out->Buffer.get(),
                    m_buffer_out->Width * m_buffer_out->Height * sizeof(RadarTrack), cudaMemcpyDeviceToHost,
                    m_cuda_stream);

    auto processed_buffer = std::vector<RadarTrack>(m_buffer_out->Width * m_buffer_out->Height);

    cudaStreamSynchronize(m_cuda_stream);

    std::vector<vec3f> points;
    m_buffer_out->Beam_return_count = 0;
    for (unsigned int i = 0; i < buf.size(); i++) {
//        if (abs(buf[i].vel[0]) > 0 || abs(buf[i].vel[1]) > 0 || abs(buf[i].vel[2]) > 0) {
        if(buf[i].intensity > 0){
            processed_buffer[m_buffer_out->Beam_return_count] = buf[i];
            points.push_back(vec3f{processed_buffer[m_buffer_out->Beam_return_count].xyz[0],
                                   processed_buffer[m_buffer_out->Beam_return_count].xyz[1],
                                   processed_buffer[m_buffer_out->Beam_return_count].xyz[2]});
            m_buffer_out->Beam_return_count++;
        }
    }

    int minimum_points = 5;
    float epsilon = 1;

#if PROFILE
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "DBSCAN initiated with " << points.size() << " points" << std::endl;

    auto dbscan = DBSCAN();
    dbscan.Run(&points, epsilon, minimum_points);

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "DBScan time = " << milli << "ms" << std::endl;
#else
    auto dbscan = DBSCAN();
    dbscan.Run(&points, epsilon, minimum_points);
#endif

    auto clusters = dbscan.getClusters();

    // calculate average velocity and centroids of each cluster

    m_buffer_out->avg_velocity.clear();
    m_buffer_out->centroids.clear();

    for (int i = 0; i < clusters.size(); i++) {
        std::array<float,3> temp = {0,0,0};
        m_buffer_out->avg_velocity.push_back(temp);
        m_buffer_out->centroids.push_back(temp);
    }

    std::vector<RadarTrack> valid_returns;
    for (int i = 0; i < clusters.size(); i++) {
        for (int j = 0; j < clusters[i].size(); j++) {
            // we are adding 1 so cluster ID starts at 1 instead of 0
            int idx = clusters[i][j];
            processed_buffer[idx].objectID = i + 1;
            valid_returns.push_back(processed_buffer[idx]);
            // adding velocity and xyz and then dividing by size in next for loop
            m_buffer_out->centroids[i][0] += processed_buffer[idx].xyz[0];
            m_buffer_out->centroids[i][1] += processed_buffer[idx].xyz[1];
            m_buffer_out->centroids[i][2] += processed_buffer[idx].xyz[2];

            m_buffer_out->avg_velocity[i][0] += processed_buffer[idx].vel[0];
            m_buffer_out->avg_velocity[i][1] += processed_buffer[idx].vel[1];
            m_buffer_out->avg_velocity[i][2] += processed_buffer[idx].vel[2];
           
        }
    }
    for (int i = 0; i < m_buffer_out->avg_velocity.size(); i++) {
        m_buffer_out->avg_velocity[i][0] = m_buffer_out->avg_velocity[i][0] / (clusters[i].size());
        m_buffer_out->avg_velocity[i][1] = m_buffer_out->avg_velocity[i][1] / (clusters[i].size());
        m_buffer_out->avg_velocity[i][2] = m_buffer_out->avg_velocity[i][2] / (clusters[i].size());
        m_buffer_out->centroids[i][0] = m_buffer_out->centroids[i][0] / (clusters[i].size());
        m_buffer_out->centroids[i][1] = m_buffer_out->centroids[i][1] / (clusters[i].size());
        m_buffer_out->centroids[i][2] = m_buffer_out->centroids[i][2] / (clusters[i].size());
    }

    m_buffer_out->invalid_returns = m_buffer_out->Beam_return_count - valid_returns.size();
    m_buffer_out->Beam_return_count = valid_returns.size();
    m_buffer_out->Num_clusters = clusters.size();
    memcpy(m_buffer_out->Buffer.get(), valid_returns.data(),
           m_buffer_out->Beam_return_count * sizeof(RadarTrack));

#if PROFILE
    printf("Scan %i\n", m_scan_number);
    m_scan_number++;
    int total_returns = m_buffer_out->Beam_return_count + m_buffer_out->invalid_returns;
    printf("Number of returns: %i |  Number of valid returns: %i | Number of clusters: %i\n", total_returns,
           m_buffer_in->Beam_return_count, clusters.size());

    // note that we are starting with i = 1 because there are no clusters with id of 0
    for (int i = 1; i <= clusters.size(); i++) {
        int count = 0;
        float3 avg_vel = {0, 0, 0};
        for (int j = 0; j < m_buffer_out->Beam_return_count; j++) {
            if (m_buffer_out->Buffer[j].objectID == i) {
                count++;
            }
        }
        printf("Cluster %i: %i returns\n", i, count);
        printf("velocity %f %f %f\n", m_buffer_out->avg_velocity[i - 1][0], m_buffer_out->avg_velocity[i - 1][1],
               m_buffer_out->avg_velocity[i - 1][2]);
        printf("centroid %f %f %f\n", m_buffer_out->centroids[i - 1][0], m_buffer_out->centroids[i - 1][1],
               m_buffer_out->centroids[i - 1][2]);
        printf("-------\n");
    }
    printf("--------------------------------------------------------\n");
#endif

    /*
    GROUND TRUTH
    */
    // calculating centroid and avg velocity of clusters, currently not in output
    //    std::vector<float3> centroids;
    //    std::vector<float3> avg_vel;
    //    std::vector<int> count;
    //    for (int i = 0; i < processed_buffer.size(); i++){
    //        while (processed_buffer[i].objectID + 1 > centroids.size()){
    //            centroids.push_back(make_float3(0,0,0));
    //            avg_vel.push_back(make_float3(0,0,0));
    //            count.push_back(0);
    //        }
    //        centroids[(int)processed_buffer[i].objectID].x += processed_buffer[i].x;
    //        centroids[(int)processed_buffer[i].objectID].y += processed_buffer[i].y;
    //        centroids[(int)processed_buffer[i].objectID].z += processed_buffer[i].z;
    //        avg_vel[(int)processed_buffer[i].objectID].x += processed_buffer[i].x_vel;
    //        avg_vel[(int)processed_buffer[i].objectID].y += processed_buffer[i].y_vel;
    //        avg_vel[(int)processed_buffer[i].objectID].z += processed_buffer[i].z_vel;
    //        count[(int)processed_buffer[i].objectID] += 1;
    //    }
    //
    //    for (int i = 0; i < centroids.size(); i++){
    //        centroids[i].x /= count[i];
    //        centroids[i].y /= count[i];
    //        centroids[i].z /= count[i];
    //        avg_vel[i].x /= count[i];
    //        avg_vel[i].y /= count[i];
    //        avg_vel[i].z /= count[i];
    //    }
    //
    //    m_buffer_out->Num_clusters = centroids.size();
    //
    //    std::cout<<m_buffer_out->Beam_return_count<<std::endl;
    //    cudaMemcpyAsync(m_buffer_out->Buffer.get(), processed_buffer.data(), m_buffer_out->Beam_return_count *
    //    sizeof(RadarTrack),
    //                    cudaMemcpyHostToDevice, m_cuda_stream);

    m_buffer_out->LaunchedCount = m_buffer_in->LaunchedCount;
    m_buffer_out->TimeStamp = m_buffer_in->TimeStamp;
}

}  // namespace sensor
}  // namespace chrono