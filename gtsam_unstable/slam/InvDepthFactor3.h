
/**
 * @file InvDepthFactor3.h
 * @brief Inverse Depth Factor
 * @author Chris Beall
 */

#pragma once

#include <gtsam/geometry/Cal3_S2.h>
#include <gtsam/nonlinear/NonlinearFactor.h>
#include <gtsam_unstable/geometry/InvDepthCamera3.h>

namespace gtsam {

/**
 * Ternary factor representing a visual measurement that includes inverse depth
 */
template<class POSE, class LANDMARK, class INVDEPTH>
class InvDepthFactor3: public gtsam::NoiseModelFactor3<POSE, LANDMARK, INVDEPTH> {
protected:

	// Keep a copy of measurement and calibration for I/O
	gtsam::Point2 measured_;					  		///< 2D measurement
	boost::shared_ptr<gtsam::Cal3_S2> K_;  ///< shared pointer to calibration object

public:

	/// shorthand for base class type
	typedef gtsam::NoiseModelFactor3<POSE, LANDMARK, INVDEPTH> Base;

	/// shorthand for this class
	typedef InvDepthFactor3<POSE, LANDMARK, INVDEPTH> This;

	/// shorthand for a smart pointer to a factor
	typedef boost::shared_ptr<This> shared_ptr;

	/// Default constructor
	InvDepthFactor3() : K_(new gtsam::Cal3_S2(444, 555, 666, 777, 888)) {}

	/**
	 * Constructor
	 * TODO: Mark argument order standard (keys, measurement, parameters)
	 * @param measured is the 2 dimensional location of point in image (the measurement)
	 * @param model is the standard deviation
	 * @param poseKey is basically the frame number
	 * @param pointKey is the index of the landmark
   * @param invDepthKey is the index of inverse depth
	 * @param K shared pointer to the constant calibration
	 */
	InvDepthFactor3(const gtsam::Point2& measured, const gtsam::SharedNoiseModel& model,
			const gtsam::Key poseKey, gtsam::Key pointKey, gtsam::Key invDepthKey, const gtsam::shared_ptrK& K) :
				Base(model, poseKey, pointKey, invDepthKey), measured_(measured), K_(K) {}

	/** Virtual destructor */
	virtual ~InvDepthFactor3() {}

	/**
	 * print
	 * @param s optional string naming the factor
	 * @param keyFormatter optional formatter useful for printing Symbols
	 */
	void print(const std::string& s = "InvDepthFactor3",
			const gtsam::KeyFormatter& keyFormatter = gtsam::DefaultKeyFormatter) const {
		Base::print(s, keyFormatter);
		measured_.print(s + ".z");
	}

	/// equals
	virtual bool equals(const gtsam::NonlinearFactor& p, double tol = 1e-9) const {
		const This *e = dynamic_cast<const This*>(&p);
		return e && Base::equals(p, tol) && this->measured_.equals(e->measured_, tol) && this->K_->equals(*e->K_, tol);
	}

	/// Evaluate error h(x)-z and optionally derivatives
	gtsam::Vector evaluateError(const POSE& pose, const gtsam::LieVector& point, const INVDEPTH& invDepth,
			boost::optional<gtsam::Matrix&> H1=boost::none,
			boost::optional<gtsam::Matrix&> H2=boost::none,
			boost::optional<gtsam::Matrix&> H3=boost::none) const {
		try {
			InvDepthCamera3<gtsam::Cal3_S2> camera(pose, K_);
			gtsam::Point2 reprojectionError(camera.project(point, invDepth, H1, H2, H3) - measured_);
			return reprojectionError.vector();
		} catch( CheiralityException& e) {
			if (H1) *H1 = gtsam::zeros(2,6);
			if (H2) *H2 = gtsam::zeros(2,5);
			if (H3) *H2 = gtsam::zeros(2,1);
			std::cout << e.what() << ": Landmark "<< DefaultKeyFormatter(this->key2()) <<
					" moved behind camera " << DefaultKeyFormatter(this->key1()) << std::endl;
			return gtsam::ones(2) * 2.0 * K_->fx();
		}
		return gtsam::Vector_(1, 0.0);
	}

	/** return the measurement */
	const gtsam::Point2& imagePoint() const {
		return measured_;
	}

	/** return the calibration object */
	inline const gtsam::Cal3_S2::shared_ptr calibration() const {
		return K_;
	}

private:

	/// Serialization function
	friend class boost::serialization::access;
	template<class ARCHIVE>
	void serialize(ARCHIVE & ar, const unsigned int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Base);
		ar & BOOST_SERIALIZATION_NVP(measured_);
		ar & BOOST_SERIALIZATION_NVP(K_);
	}
};
} // \ namespace gtsam
