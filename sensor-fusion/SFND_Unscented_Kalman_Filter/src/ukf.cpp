#include "ukf.h"
#include "Eigen/Dense"
#include "iostream"
using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 3.;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 2.;
  
  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  
  /**
   * End DO NOT MODIFY section for measurement noise values 
   */
  
  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */
    n_x_ = 5;
    n_aug_ = 7;
    gXSigma_mat = MatrixXd(n_x_, 2 * n_aug_ + 1);

    // Define spreading parameter for augmentation
    lambda_ = 3 - n_aug_;

    weights_ = VectorXd(2 * n_aug_ + 1);
    weights_.fill(0.5 / (lambda_ + n_aug_));
    weights_(0) = lambda_ / (lambda_ + n_aug_);

    gR_lidar = MatrixXd(2, 2);
    gR_lidar.fill(0.0);
    gR_lidar(0, 0) = std_laspx_ * std_laspx_;
    gR_lidar(1, 1) = std_laspy_ * std_laspy_;

    // add measurement noise covariance matrix to have diagonal values.
    gR_radar = MatrixXd(3, 3);
    gR_radar.fill(0.0);
    gR_radar(0, 0) = std_radr_ * std_radr_;
    gR_radar(1, 1) = std_radphi_ * std_radphi_;
    gR_radar(2, 2) = std_radrd_ * std_radrd_;

  	// update later for init. 
    is_initialized_ = false;
    //std::cout<<"Initialization complete"<<endl;
}

UKF::~UKF() {}

/* ProcessMeasurement
- Check if the filter is initialized**; if not, proceed with initialization.
- Initialize state based on sensor type** (Radar or Lidar) using the first measurement.
- Set initial state vector (`x_`) and covariance matrix (`P_`)
- Update the timestamp** and mark the filter as initialized.
- Predict the state and update it based on the new sensor measurement */
  
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
    if (!is_initialized_) {
        if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
            double rho = meas_package.raw_measurements_(0);
            double phi = meas_package.raw_measurements_(1);
            double rho_d = meas_package.raw_measurements_(2);

            double p_x = rho * cos(phi);
            double p_y = rho * sin(phi);

            double vx = rho_d * cos(phi);
            double vy = rho_d * sin(phi);
            double v = sqrt(vx * vx + vy * vy);

            x_ << p_x, p_y, v,0, 0;
            P_ << std_radr_ * std_radr_, 0, 0, 0, 0,
                    0, std_radr_ * std_radr_, 0, 0, 0,
                    0, 0, std_radrd_ * std_radrd_, 0, 0,
                    0, 0, 0, std_radphi_, 0,
                    0, 0, 0, 0, std_radphi_;
        } else {
            x_ << meas_package.raw_measurements_(0), meas_package.raw_measurements_(1), 0., 0, 0;
            P_ << std_laspx_ * std_laspx_, 0, 0, 0, 0,
                    0, std_laspy_ * std_laspy_, 0, 0, 0,
                    0, 0, 1, 0, 0,
                    0, 0, 0, 1, 0,
                    0, 0, 0, 0, 1;
        }

        is_initialized_ = true;

        //std::cout << "X = " <<x_<<endl;
        //std::cout << "P_ = " <<P_ << endl;

        time_us_ = meas_package.timestamp_;
        //std::cout << "Initialization Done" << std::endl;
        return;
    }

    double dt = (meas_package.timestamp_ - time_us_)/1000000.0;
    time_us_ = meas_package.timestamp_;

    // predict step
    Prediction(dt);
    //std::cout << "Prediction done" << std::endl;
  
    // update step
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_) {
        UpdateRadar(meas_package);
        //std::cout << "Update Radar done" << std::endl;

    } else if (meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_) {
        UpdateLidar(meas_package);
        //std::cout << "Update Lidar done" << std::endl;
    }
}
/*
1. Augment the state vector and covariance matrix with process noise.
2. Generate sigma points around the augmented state.
3. Predict sigma points using the process model over time `delta_t`.
4. Handle potential division by zero in the motion model.
5. Incorporate process noise into the predicted sigma points.
6. Calculate the predicted state mean using weighted sigma points.
7. Update the predicted state covariance matrix based on sigma point spread.
8. Normalize angles in the state difference for consistency.
9. Finalize the state and covariance prediction.
10. Prepare the predicted state for the next measurement update.
*/
void UKF::Prediction(double delta_t) {
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */
    // create sigma point matrix
    MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
    Xsig_aug.fill(0.0);
  
   // create augmented mean vector
    VectorXd x_aug = VectorXd(n_aug_);
    x_aug.head(n_x_) = x_;
    x_aug(5) = 0;
    x_aug(6) = 0;
  
    // create augmented state covariance
    MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
    P_aug.fill(0.0);
    P_aug.topLeftCorner(n_x_, n_x_) = P_;
    P_aug(5, 5) = std_a_ * std_a_;
    P_aug(6, 6) = std_yawdd_ * std_yawdd_;


    // create square root matrix
    MatrixXd L = P_aug.llt().matrixL();

    // create augmented sigma points
    Xsig_aug.col(0) = x_aug;
    for (int i = 0; i < n_aug_; ++i) {
        Xsig_aug.col(i + 1) = x_aug + sqrt(lambda_ + n_aug_) * L.col(i);
        Xsig_aug.col(i + 1 + n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * L.col(i);
    }

    // predict sigma points
    for (int i = 0; i < 2*n_aug_ + 1; ++i) {
        // extract values for better readability
        double p_x = Xsig_aug(0, i);
        double p_y = Xsig_aug(1, i);
        double v = Xsig_aug(2, i);
        double yaw = Xsig_aug(3, i);
        double dyaw = Xsig_aug(4, i);
        double nu_a = Xsig_aug(5, i);
        double dd_nu_yaw = Xsig_aug(6, i);
        double v_p = v;
        double yaw_p = yaw + dyaw * delta_t;
        double dyaw_p = dyaw;
        double px_p, py_p;

        // avoid division by zero
        if (fabs(dyaw) > 0.001) {
            px_p = p_x + v / dyaw * (sin(yaw + dyaw * delta_t) - sin(yaw));
            py_p = p_y + v / dyaw * (cos(yaw) - cos(yaw + dyaw * delta_t));
        } else {
            px_p = p_x + v * delta_t * cos(yaw);
            py_p = p_y + v * delta_t * sin(yaw);
        }

        // add noise
        px_p = px_p + 0.5 * nu_a * delta_t * delta_t * cos(yaw);
        py_p = py_p + 0.5 * nu_a * delta_t * delta_t * sin(yaw);
        v_p = v_p + nu_a * delta_t;

        yaw_p = yaw_p + 0.5 * dd_nu_yaw * delta_t * delta_t;
        dyaw_p = dyaw_p + dd_nu_yaw * delta_t;

        // write predicted sigma point into right column
        gXSigma_mat(0, i) = px_p;
        gXSigma_mat(1, i) = py_p;
        gXSigma_mat(2, i) = v_p;
        gXSigma_mat(3, i) = yaw_p;
        gXSigma_mat(4, i) = dyaw_p;
    }

    //create vector for predicted state
    x_.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; ++i) {
        x_ = x_ + weights_(i) * gXSigma_mat.col(i);
    }

    // create covariance matrix for prediction
    P_.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; ++i) {
        VectorXd x_diff = gXSigma_mat.col(i) - x_;

        while (x_diff(3) > M_PI) x_diff(3) -= 2. * M_PI;
        while (x_diff(3) < -M_PI) x_diff(3) += 2. * M_PI;

        P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
    }
}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use lidar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */
    // set measurement dimension, px,py
    int n_z_ = 2;

    // create example vector for incoming lidar measurement
    VectorXd z = VectorXd(n_z_);
    z << meas_package.raw_measurements_(0),
            meas_package.raw_measurements_(1);

    // create matrix for sigma points in measurement space
    MatrixXd Zsig = MatrixXd(n_z_, 2 * n_aug_ + 1);

    // mean predicted measurement
    VectorXd z_pred = VectorXd(n_z_);
    z_pred.fill(0.0);

    // measurement covariance matrix S
    MatrixXd S = MatrixXd(n_z_, n_z_);
    S.fill(0.0);

    for (int i = 0; i < 2 * n_aug_ + 1; ++i) {
        double p_x = gXSigma_mat(0, i);
        double p_y = gXSigma_mat(1, i);

        // measurement model
        Zsig(0, i) = p_x;
        Zsig(1, i) = p_y;
        // mean predicted measurement
        z_pred += weights_(i) * Zsig.col(i);
    }

    // innovation covariance matrix S
    for (int i = 0; i < 2 * n_aug_ + 1; ++i) {
        VectorXd z_diff = Zsig.col(i) - z_pred;

        S += weights_(i) * z_diff * z_diff.transpose();
    }

    S += gR_lidar;

    // create matrix for cross correlation Tc
    MatrixXd Tc = MatrixXd(n_x_, n_z_);
    Tc.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; ++i) {
        VectorXd z_diff = Zsig.col(i) - z_pred;
        VectorXd x_diff = gXSigma_mat.col(i) - x_;

        // normalize angles
//         while (x_diff(3) > M_PI) x_diff(3) -= 2 * M_PI;
//         while (x_diff(3) < -M_PI) x_diff(3) += 2 * M_PI;

        Tc += weights_(i) * x_diff * z_diff.transpose();
    }

    //  Kalman gain K;
    MatrixXd K = Tc * S.inverse();

    // residual
    VectorXd z_diff = z - z_pred;

    // update state mean and covariance matrix
    x_ += K * z_diff;
    P_ -= K * S * K.transpose();
}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use radar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */

    int n_z_ = 3;
    VectorXd z = VectorXd(n_z_);
    double meas_rho = meas_package.raw_measurements_(0);
    double meas_phi = meas_package.raw_measurements_(1);
    double meas_rhod = meas_package.raw_measurements_(2);
  
    z << meas_rho,
            meas_phi,
            meas_rhod;

    // create matrix for sigma points in measurement space
    MatrixXd Zsig = MatrixXd(n_z_, 2 * n_aug_ + 1);

    // mean predicted measurement
    VectorXd z_pred = VectorXd(n_z_);
    z_pred.fill(0.0);
    // measurement covariance matrix S
    MatrixXd S = MatrixXd(n_z_, n_z_);
    S.fill(0.0);

    // transform sigma points into measurement space
    for (int i = 0; i < 2 * n_aug_ + 1; ++i) {
        // 2n+1 simga points
        // extract values for better readability
        double p_x = gXSigma_mat(0, i);
        double p_y = gXSigma_mat(1, i);
        double v = gXSigma_mat(2, i);
        double yaw = gXSigma_mat(3, i);
        double v1 = cos(yaw) * v;
        double v2 = sin(yaw) * v;

        // measurement model
        Zsig(0, i) = sqrt(p_x * p_x + p_y * p_y); //r
        Zsig(1, i) = atan2(p_y, p_x); // phi
        Zsig(2, i) = (p_x * v1 + p_y * v2) / sqrt(p_x * p_x + p_y * p_y); //r_dot

        //calculate mean predicted measurement
        z_pred += weights_(i) * Zsig.col(i);
    }

    //  S: covariance matrix 
    for (int i = 0; i < 2 * n_aug_ + 1; ++i) {
        VectorXd z_diff = Zsig.col(i) - z_pred;

        while (z_diff(1) > M_PI) z_diff(1) -= 2. * M_PI;
        while (z_diff(1) < -M_PI) z_diff(1) += 2. * M_PI;

        S += weights_(i) * z_diff * z_diff.transpose();
    }

    S += gR_radar;

    // Fill cross correlation Tc matrix  
    MatrixXd Tc = MatrixXd(n_x_, n_z_);
    Tc.fill(0.0);

  	// Angle normalization and Z sigma
    for (int i = 0; i < 2 * n_aug_ + 1; ++i) {
        VectorXd z_diff = Zsig.col(i) - z_pred;
        while (z_diff(1) > M_PI) z_diff(1) -= 2. * M_PI;
        while (z_diff(1) < -M_PI) z_diff(1) += 2. * M_PI;

        VectorXd x_diff = gXSigma_mat.col(i) - x_;
        while (x_diff(3) > M_PI) x_diff(3) -= 2. * M_PI;
        while (x_diff(3) < -M_PI) x_diff(3) += 2. * M_PI;

        Tc += weights_(i) * x_diff * z_diff.transpose();
    }

    //  Kalman gain K;
    MatrixXd K = Tc * S.inverse();

    // residual
    VectorXd z_diff = z - z_pred;

    // angle normalization
    while (z_diff(1) > M_PI) z_diff(1) -= 2. * M_PI;     
    while (z_diff(1) < -M_PI) z_diff(1) += 2. * M_PI;

    // update state mean and covariance matrix
    x_ += K * z_diff;
    P_ -= K * S * K.transpose();
}