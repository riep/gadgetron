/** \file   EPIReconXObjectTrapezoid.h
    \brief  Implement functionality for EPI X reconstruction operator for Trapezoidal type
    \author Souheil Inati
*/

#pragma once

#include "EPIExport.h"
#include "EPIReconXObject.h"
#include "hoArmadillo.h"
#include "hoNDArray_elemwise.h"
#include "gadgetronmath.h"
#include <complex>

namespace Gadgetron { namespace EPI {

template <typename T> class EPIReconXObjectTrapezoid : public EPIReconXObject<T>
{
 public:
  EPIReconXObjectTrapezoid();
  virtual ~EPIReconXObjectTrapezoid();

  virtual int computeTrajectory();

  virtual int apply(const ISMRMRD::AcquisitionHeader &hdr_in, const hoNDArray <T> &data_in, 
		    ISMRMRD::AcquisitionHeader &hdr_out, hoNDArray <T> &data_out);

  using EPIReconXObject<T>::filterPos_;
  using EPIReconXObject<T>::filterNeg_;
  using EPIReconXObject<T>::slicePosition;
  using EPIReconXObject<T>::rcvType_;

  float rampUpTime_;
  float rampDownTime_;
  float flatTopTime_;
  float acqDelayTime_;
  int   numSamples_;
  float dwellTime_;
  int   encodeNx_;
  float encodeFOV_;
  int   reconNx_;
  float reconFOV_;

 protected:
  using EPIReconXObject<T>::trajectoryPos_;
  using EPIReconXObject<T>::trajectoryNeg_;

  hoNDArray <T> Mpos_;
  hoNDArray <T> Mneg_;
  bool operatorComputed_;

};

template <typename T> EPIReconXObjectTrapezoid<T>::EPIReconXObjectTrapezoid()
{
  rcvType_ = EVEN;
  rampUpTime_ = 0.0;
  rampDownTime_ = 0.0;
  flatTopTime_ = 0.0;
  acqDelayTime_ = 0.0;
  numSamples_ = 0.0;
  dwellTime_ = 0.0;
  encodeNx_ = 0;
  reconNx_ = 0;
  encodeFOV_ = 0.0;
  reconFOV_ = 0.0;
  operatorComputed_ = false;
}

template <typename T> EPIReconXObjectTrapezoid<T>::~EPIReconXObjectTrapezoid()
{
}

template <typename T> int EPIReconXObjectTrapezoid<T>::computeTrajectory()
{

  // Initialize the k-space trajectory arrays
  trajectoryPos_.create(numSamples_);
  Gadgetron::clear(trajectoryPos_);
  trajectoryNeg_.create(numSamples_);
  Gadgetron::clear(trajectoryNeg_);

  // Temporary trajectory for a symmetric readout that is one time point longer
  // first calculate the integral with G = 1;
  int nK = numSamples_+1;
  hoNDArray <float> k(nK);
  float t;
  int n;

  std::cout << "Dwell = " << dwellTime_ << "    acqDelayTime = " << acqDelayTime_ << std::endl;
  std::cout << "rampUpTime = " << rampUpTime_ << "    flatTopTime = " << flatTopTime_ << "    rampDownTime = " << rampDownTime_ << std::endl;

  for (n=0; n<nK; n++)
  {  
    t = (n+0.5)*dwellTime_ + acqDelayTime_;  // half-way through the dwell time
    if (t <= rampUpTime_) {
      // on the ramp up
      k[n] = 0.5 / rampUpTime_ * t*t;
    }
    else if ((t > rampUpTime_) && (t <= (rampUpTime_+flatTopTime_))) {
      // on the flat top
      k[n] = 0.5*rampUpTime_ + (t - rampUpTime_);
    }
    else {
      // on the ramp down
      float v = (rampUpTime_+flatTopTime_+rampDownTime_-t);
      k[n] = 0.5*rampUpTime_ + flatTopTime_ + 0.5*rampDownTime_ - 0.5/rampDownTime_*v*v;
    }
    std::cout << n << ":  " << t << "  " << k[n] << std::endl;
  }

  // Scale the trajectory to match the desired total integral
  // shift it so that the center point is at k=0
  float scale = encodeNx_ / (k[nK-1] - k[0]);
  float shift = k[nK/2];
  for (n=0; n<nK; n++)
  {
    k[n] = (k[n] - shift ) * scale;
    std::cout << n << ":  " << k[n] << std::endl;
  }


  // Fill the positive and negative trajectories
  for (n=0; n<numSamples_; n++)
  {
    trajectoryPos_[n] = k[n];
    trajectoryNeg_[n] = k[nK-1 - n];
  }

  // reset the operatorComputed_ flag
  operatorComputed_ = false;

  return(0);
}


template <typename T> int EPIReconXObjectTrapezoid<T>::apply(const ISMRMRD::AcquisitionHeader &hdr_in, const hoNDArray <T> &data_in, 
		    ISMRMRD::AcquisitionHeader &hdr_out, hoNDArray <T> &data_out)
{
  if (!operatorComputed_) {
    // Compute the reconstruction operator
    int Km = floor(encodeNx_ / 2.0);
    int Ne = 2*Km + 1;
    int p,q; // counters

    // resize the reconstruction operator
    Mpos_.create(reconNx_,numSamples_);
    Mneg_.create(reconNx_,numSamples_);

    // evenly spaced k-space locations
    arma::vec keven = arma::linspace<arma::vec>(-Km, Km, Ne);
    //keven.print("keven =");

    // image domain locations [-0.5,...,0.5)
    arma::vec x = arma::linspace<arma::vec>(-0.5,(reconNx_-1.)/(2.*reconNx_),reconNx_);
    //x.print("x =");

    // DFT operator
    arma::cx_mat F(reconNx_, Ne);
    for (p=0; p<reconNx_; p++) {
      for (q=0; q<Ne; q++) {
	F(p,q) = 1.0/std::sqrt(Ne) * std::exp(std::complex<double>(0.0,-1.0*2*M_PI*keven(q)*x(p)));
      }
    }
    //F.print("F =");

    // forward operators
    arma::mat Qp(numSamples_, Ne);
    arma::mat Qn(numSamples_, Ne);
    for (p=0; p<numSamples_; p++) {
      //std::cout << trajectoryPos_(p) << "    " << trajectoryNeg_(p) << std::endl;
      for (q=0; q<Ne; q++) {
	Qp(p,q) = sinc(trajectoryPos_(p)-keven(q));
	Qn(p,q) = sinc(trajectoryNeg_(p)-keven(q));
      }
    }

    //Qp.print("Qp =");
    //Qn.print("Qn =");

    // recon operators
    arma::cx_mat Mp(reconNx_,numSamples_);
    arma::cx_mat Mn(reconNx_,numSamples_);
    Mp = F * arma::pinv(Qp);
    Mn = F * arma::pinv(Qn);
    for (p=0; p<reconNx_; p++) {
      for (q=0; q<numSamples_; q++) {
        Mpos_(p,q) = Mp(p,q);
        Mneg_(p,q) = Mn(p,q);
      }
    }
    
    //Mp.print("Mp =");
    //Mn.print("Mn =");

    // set the operator computed flag
    operatorComputed_ = true;
  }

  // convert to armadillo representation of matrices and vectors
  arma::Mat<typename stdType<T>::Type> adata_in = as_arma_matrix(&data_in);
  arma::Mat<typename stdType<T>::Type> adata_out = as_arma_matrix(&data_out);

  
  // Apply it
  if (ISMRMRD::FlagBit(ISMRMRD::ACQ_IS_REVERSE).isSet(hdr_in.flags)) {
    // Negative readout
    adata_out = as_arma_matrix(&Mneg_) * adata_in;
  } else {
    // Forward readout
    adata_out = as_arma_matrix(&Mpos_) * adata_in;
  }

  // Copy the input header to the output header and set the size
  hdr_out = hdr_in;
  hdr_out.number_of_samples = reconNx_;
  
  return 0;
}

}}
