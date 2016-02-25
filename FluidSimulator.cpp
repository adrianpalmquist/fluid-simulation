//
// Created by Niclas Olmenius on 23/02/16.
//

#include <complex>
#include <iostream>
#include "FluidSimulator.h"


FluidSimulator::FluidSimulator() {


}


void FluidSimulator::project() {

    double t;


//  Apply precon

    applyPrecon();

    _s = _z;

    double maxError = *std::max_element(_rhs.begin(), _rhs.end(), absCompare);

    if (maxError < 1e-5)
        return;

    double sigma = *glm::dot(_rhs.data(), _z.data());

// Iterative solver

    for (int iter = 0; iter < _ITERLIMIT; ++iter) {

// Matrix vector product

        for (int y = 0, idx = 0; y < _ny; y++) {
            for (int x = 0; x < _nx; x++, idx++) {
                t = _Adiag[idx] * _s[idx];
                if (x > 0)
                    t = t + _Aplusi[idx - 1] * _s[idx - 1];
                else if (y > 0)
                    t = t + _Aplusj[idx - _nx] * _s[idx - _nx];
                else if (x < _nx - 1)
                    t = t + _Aplusi[idx] * _s[idx + 1];
                else if (y < _ny - 1)
                    t = t + _Aplusj[idx] * _s[idx + _nx];

                _z[idx] = t;
            }
        }

        double alpha = sigma / *glm::dot(_z.data(), _s.data());

//  Scaled add

        scaleAdd(_pressure, _pressure, _s, alpha);
        scaleAdd(_rhs, _rhs, _z, -alpha);

        double maxError = *std::max_element(_rhs.begin(), _rhs.end(), absCompare);

        if (maxError < 1e-5) {
            std::cout << "Exiting solver after " << iter << " iterarions, maximum error is " << maxError << std::endl;
            return;
        }

//  Apply precon

        applyPrecon();

        double sigmaNew = *glm::dot(_rhs.data(), _z.data());

        scaleAdd(_s, _z, _s, sigmaNew / sigma);
        sigma = sigmaNew;

        std::cout << "Exceeded budget of " << _ITERLIMIT << " iterations, maximum error was " << maxError << std::endl;

    }

}

void FluidSimulator::addInFlow(int x0, int y0, int x1, int y1, int w, int h, int ox, int oy, double dxy, double value,
                               std::unique_ptr<std::vector<double>> src) {

}

void FluidSimulator::update(double dt) {

    //double umax = std::max(std::max(std::max(_u)),std::max(std::max(_v+sqrt(5*_dxy*abs(_gravity)))));
    //_dt = _dxy/umax;

    applyBuoyancy();
    buildRhs();
    buildPressureMatrix();
    buildPrecon();
    project();
    applyPressure();
    advect();


}

void FluidSimulator::applyPressure() {

    // Apply pressure

    double scale = _dt / (_RHO * _dxy);
    int uvidx = 0;

    for (int y = 0, idx = 0; y < _ny; y++) {
        for (int x = 0; x < _nx; x++, idx++) {
            uvidx = getIdx(x, y, _nx + 1);
            _u[uvidx] = _u[uvidx] - scale * _u[idx];
            _u[uvidx + 1] = _u[uvidx + 1] + scale * _u[idx];
            uvidx = getIdx(x, y, _nx);
            _v[uvidx] = _v[uvidx] - scale * _u[idx];
            _v[uvidx + _nx] = _v[uvidx + _nx] + scale * _u[idx];
        }
    }

    // Update boundaries

    for (int y = 0, idx = 0; y < _ny; y++, idx++) {
        idx = getIdx(1, y, _nx + 1);
        _u[idx] = 0.0;
        _u[idx + 1] = 0.0;
        idx = getIdx(_nx, y, _nx + 1);
        _u[idx] = 0.0;
        _u[idx + 1] = 0.0;
    }


    for (int x = 0, idx = 0; x < _nx; x++, idx++) {
        idx = getIdx(x, 1, _nx);
        _v[idx] = 0.0;
        _v[idx + 1] = 0.0;
        idx = getIdx(x, _ny, _nx);
        _v[idx] = 0.0;
        _v[idx + _nx] = 0.0;
    }


    for (int y = 0, idx = 0; y < _ny; y++, idx++) {
        idx = getIdx(1, y, _nx);
        _T[idx] = TAMB;
        idx = getIdx(_nx, y, _nx);
        _T[idx] = TAMB;
    }


    for (int x = 0, idx = 0; x < _nx; x++, idx++) {
        idx = getIdx(x, 1, _nx);
        _T[idx] = TAMB;
        idx = getIdx(x, _ny, _nx);
        _T[idx] = TAMB;
    }
}

void FluidSimulator::buildRhs() {

    double scale = 1 / _dxy;

    for (int y = 0, idx = 0; y < _ny; y++) {
        for (int x = 0; x < _nx; x++, idx++) {
            _rhs[idx] = -scale * ((_u[getIdx(x + 1, y, _nx + 1)] - _u[getIdx(x, y, _nx + 1)])
                                  + (_v[getIdx(x, y + 1, _ny)] - _v[getIdx(x, y, _ny)]));

        }

    }

}

void FluidSimulator::buildPrecon() {

    for (int y = 0, idx = 0; y < _ny; y++) {
        for (int x = 0; x < _nx; x++, idx++) {

            double e = _Adiag[idx];

            if (x > 0) {
                double px = _Aplusi[idx - 1] * _precon[idx - 1];
                double py = _Aplusj[idx - 1] * _precon[idx - 1];

                e = e - (px * px + _tau_mic * px * py);
            }
            if (y > 0) {
                double px = _Aplusi[idx - _nx] * _precon[idx - _nx];
                double py = _Aplusj[idx - _nx] * _precon[idx - _nx];
                e = e - (py * py + _tau_mic * px * py);
            }

            if (e < _sigma_mic * _Adiag[idx])
                e = _Adiag[idx];

            _precon[idx] = 1.0 / sqrt(e);

        }

    }
}

void FluidSimulator::applyPrecon() {

    for (int y = 0, idx = 0; y < _ny; y++) {
        for (int x = 0; x < _nx; x++, idx++) {
            double t = _rhs[idx];

            if (x > 1) {
                t = t - _Aplusi[idx - 1] * _precon[idx - 1] * _z[idx - 1];
            }

            if (y > 1) {
                t = t - _Aplusj[idx - _nx] * _precon[idx - _nx] * _z[idx - _nx];
            }

            _z[idx] = t * _precon[idx];

        }
    }

    for (int y = _ny - 1, idx = _nx * _ny - 1; y >= 0; y--) {
        for (int x = _nx - 1; x >= 0; x--, idx--) {
            double t = _z[idx];

            if (x < _nx) {
                t = t - _Aplusi[idx] * _precon[idx] * _z[idx + 1];
            }
            if (y < _ny) {
                t = t - _Aplusj[idx] * _precon[idx] * _z[idx + _nx];
            }

            _z[idx] = t * _precon[idx];
        }
    }
}

void FluidSimulator::applyBuoyancy() {


    double alpha = (_DENSITYSOOT - _DENSITYAIR) / _DENSITYAIR;

    for (int y = 0, idx = 0; y < _ny; y++) {
        for (int x = 0; x < _nx; x++) {

            double buoyancy = _dt * _GRAVITY * (alpha * _d[idx] - (_T[idx] - TAMB) / TAMB);

            _v[idx] += buoyancy * 0.5;
            _v[idx + _nx] += buoyancy * 0.5;

        }
    }


}

// NOT DONE
void FluidSimulator::advect(double tStep, const FluidSimulator &u, const FluidSimulator &v) {
    int index = 0;
    double x = 0, y = 0;
    double x = 0 , y = 0, x0 = 0, y0 = 0;

    for (int idY = 0; idY < _ny; idY++) {
        for (int idX = 0; idX < _nx; idX++) {

            //offset
            x = idX + 0.5;
            y = idY + 0.5;

            x0 = rungeKutta3(x, y, tStep, u, v);
            y0 = rungeKutta3(x, y, tStep, u, v);

            _dn[index] = cerp2(x0, y0, _nx, _ny, 0.5, 0.5, u, v);

            _dn[index] = cerp2(x0, y0,_nx,_ny,0.5,0.5,_d );
            _dn[index] = max(0, _dn * exp(-KDISS * _dt));
            _Tn[index] = cerp2(x0, y0, _nx, _ny, 0.5, 0.5, _T);
            index++;
        }
    }

    index = 0;

    for( int idY = 0 ; idY < _ny ; idY++) {
        for (int idX = 0; idX <= _nx; idX++) {

            //offset
            x = idX + 0.5;
            y = idY + 0.5;

            x0 = rungeKutta3(x, y, tStep, u, v);
            y0 = rungeKutta3(x, y, tStep, u, v);

            _un[index] = cerp2(x0, y0, (_nx + 1), _ny, 0.0, 0.5, u);
            index++;

        }
    }

    index = 0;

    for( int idY = 0 ; idY <= _ny ; idY++) {
        for (int idX = 0; idX < _nx; idX++) {

            //offset

            x0 = rungeKutta3(x, y, tStep, u, v);
            y0 = rungeKutta3(x, y, tStep, u, v);

            _vn[index] = cerp2(x0, y0, _nx, (_ny + 1), 0.5, 0.0, v);
            index++;

        }
    }

    //update the vectors
    _d = _dn;
    _T = _Tn;
    _u = _un;
    _v = _vn;
}


void FluidSimulator::rungeKutta3(double &x, double &y, double tStep, const std::vector<double> &u,
                                 const std::vector<double> &v) {
    double earlyU = u.lerp2(x, y) / _dxy;
    double earlyV = v.lerp2(x, y) / _dxy;

    double mX = x - (0.5 * tStep * earlyU);
    double mY = y - (0.5 * tStep * earlyV);

    double mU = u.lerp2(mX, mY) / _dxy;
    double mV = v.lerp2(mX, mY) / _dxy;

    double lateX = x - (0.75 * tStep * mU);
    double lateY = y - (0.75 * tStep * mV);

    double lateU = u.lerp2(lateX, lateY);
    double lateV = v.lerp2(lateX, lateY);

    x -= tStep * ((2.0 / 9.0) * earlyU + (3.0 / 9.0) * mU + (4.0 / 9.0) * lateU);
    y -= tStep * (((2.0 / 9.0) * earlyV + (3.0 / 9.0) * mV + (4.0 / 9.0) * lateV));
}


double FluidSimulator::cerp2(double x, double y, int w, int h, double ox, double oy, std::vector<double> &quantity) {

    x = std::min(std::max(x - ox, 0.0), w - 1.001);
    y = std::min(std::max(y - oy, 0.0), h - 1.001);
    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    x = x - ix;
    y = y - iy;

    int x0 = std::max(ix - 1, 0);
    int x1 = ix;
    int x2 = ix + 1;
    int x3 = std::min(ix + 2, w - 1);

    int y0 = std::max(iy - 1, 0);
    int y1 = iy;
    int y2 = iy + 1;
    int y3 = std::min(iy + 2, h - 1);

    double q0 = cerp(quantity[getIdx(x0, y0, w)], quantity[getIdx(x1, y0, w)],
                     quantity[getIdx(x2, y0, w)], quantity[getIdx(x3, y0, w)], x);
    double q1 = cerp(quantity[getIdx(x0, y1, w)], quantity[getIdx(x1, y1, w)],
                     quantity[getIdx(x2, y1, w)], quantity[getIdx(x3, y1, w)], x);
    double q2 = cerp(quantity[getIdx(x0, y2, w)], quantity[getIdx(x1, y2, w)],
                     quantity[getIdx(x2, y2, w)], quantity[getIdx(x3, y2, w)], x);
    double q3 = cerp(quantity[getIdx(x0, y3, w)], quantity[getIdx(x1, y3, w)],
                     quantity[getIdx(x2, y3, w)], quantity[getIdx(x3, y3, w)], x);

    return cerp(q0, q1, q2, q3, y);

}

double FluidSimulator::lerp(double a, double b, double x) {

    double xy = a * (1.0 - x) + b * x;

    return xy;
}

double FluidSimulator::cerp(double a, double b, double c, double d, double x) {

    double xsq = x * x;
    double xcu = xsq * x;
    double minV = std::min(a, std::min(b, std::min(c, d)));
    double maxV = std::max(a, std::max(b, std::max(c, d)));

    double t =
            a * (0.0 - 0.5 * x + 1.0 * xsq - 0.5 * xcu) +
            b * (1.0 + 0.0 * x - 2.5 * xsq + 1.5 * xcu) +
            c * (0.0 + 0.5 * x + 2.0 * xsq - 1.5 * xcu) +
            d * (0.0 + 0.0 * x - 0.5 * xsq + 0.5 * xcu);

    return std::min(std::max(t, minV), maxV);

}


static bool absCompare(double a, double b) {
    return (std::abs(a) < std::abs(b));
}

void FluidSimulator::scaleAdd(std::vector<double> &curr, std::vector<double> &a, std::vector<double> &b, double s) {
    for (int i = 0; i < _nx * _ny; ++i) {
        curr[i] = a[i] + b[i] * s;
    }
}

void FluidSimulator::buildPressureMatrix() {

    // Set up matrix entities for the pressure equations
    double scale = _dt / (_RHO * _dxy * _dxy);
    std::fill(_Adiag.begin(), _Adiag.end(), 0.0); // Coeffcient matrix for pressure equations
    std::fill(_Adiag.begin(), _Adiag.end(), 0.0);
    std::fill(_Adiag.begin(), _Adiag.end(), 0.0);

    for (int y = 0, idx = 0; y < _ny; y++) {
        for (int x = 0; x < _nx; x++, idx++) {

            if (x < _nx - 1) {
                _Adiag[idx] += scale;
                _Adiag[idx + 1] += scale;
                _Aplusi[idx] = (-scale);
            } else {
                _Aplusi[idx] = 0.0;
            }

            if (y < _ny - 1) {
                _Adiag[idx] += scale;
                _Adiag[idx + _nx] += scale;
                _Aplusj[idx] = (-scale);
            } else {
                _Aplusj[idx] = 0.0;
            }

        }
    }


}

double FluidSimulator::lerp2(double x, double y, double ox, double oy, int w, int h, std::vector<double> &quantity) {

    x = std::min(std::max(x - ox, 1.0), w - 1.001);
    y = std::min(std::max(y - oy, 1.0), h - 1.001);
    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    x = x - ix;
    y = y - iy;

    double x00 = quantity[getIdx(ix, iy, w)];
    double x10 = quantity[getIdx(ix + 1, iy, w)];
    double x01 = quantity[getIdx(ix, iy + 1, w)];
    double x11 = quantity[getIdx(ix + 1, iy + 1, w)];

    return lerp(lerp(x00, x10, x), lerp(x01, x11, x), y);


}
