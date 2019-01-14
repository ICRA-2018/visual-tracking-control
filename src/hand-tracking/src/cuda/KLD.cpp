#include <KLD.h>

#include <device_likelihood.h>

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <utility>
#include <vector>

using namespace bfl;
using namespace Eigen;



struct KLD::ImplData
{
    cublasHandle_t handle_;

    double likelihood_gain_;

    std::size_t vector_size_;
};


KLD::KLD(const double likelihood_gain, const std::size_t vector_size) noexcept :
    pImpl_(std::unique_ptr<ImplData>(new ImplData))
{
    ImplData& rImpl = *pImpl_;


    rImpl.likelihood_gain_ = likelihood_gain;

    rImpl.vector_size_ = vector_size;

    cublasCreate(&(rImpl.handle_));
}


KLD::~KLD() noexcept
{
    ImplData& rImpl = *pImpl_;

    cublasDestroy(rImpl.handle_);
}


std::pair<bool, VectorXd> KLD::likelihood(const MeasurementModel& measurement_model, const Ref<const MatrixXd>& pred_states)
{
    ImplData& rImpl = *pImpl_;


    bool valid_measurements;
    Data data_measurements;
    std::tie(valid_measurements, data_measurements) = measurement_model.measure();

    if (!valid_measurements)
        return std::make_pair(false, VectorXd::Zero(1));

    cv::cuda::GpuMat measurements = any::any_cast<cv::cuda::GpuMat>(data_measurements);


    bool valid_predicted_measurements;
    Data data_predicted_measurements;
    std::tie(valid_predicted_measurements, data_predicted_measurements) = measurement_model.predictedMeasure(pred_states);

    cv::cuda::GpuMat predicted_measurements = any::any_cast<cv::cuda::GpuMat>(data_predicted_measurements);

    if (!valid_predicted_measurements)
        return std::make_pair(false, VectorXd::Zero(1));

    thrust::host_vector<float> device_normtwo_kld = bfl::cuda::kld(rImpl.handle_,
                                                                   measurements, predicted_measurements,
                                                                   rImpl.vector_size_);

    Map<VectorXf> likelihood_float(device_normtwo_kld.data(), device_normtwo_kld.size());
    VectorXd likelihood = (-rImpl.likelihood_gain_ * likelihood.cast<double>()).array().exp();


    return std::make_pair(true, std::move(likelihood));
}
