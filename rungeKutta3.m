function [ x_out, y_out ] = rungeKutta3( x, y, ox, oy, dt, u, v, dxy, w, h )
%RK3 Summary of this function goes here
%   Detailed explanation goes here

        firstU = lerp2(x, y, ox, oy, w, h, u)/dxy;
        firstV = lerp2(x, y, ox, oy, w ,h ,v)/dxy;

        midX = x - 0.5*dt*firstU;
        midY = y - 0.5*dt*firstV;

        midU = lerp2(midX, midY, ox, oy, w, h, u)/dxy;
        midV = lerp2(midX, midY, ox, oy, w, h, v)/dxy;

        lastX = x - 0.75*dt*midU;
        lastY = y - 0.75*dt*midV;

        lastU = lerp2(lastX, lastY, ox, oy, w, h, u);
        lastV = lerp2(lastX, lastY, ox, oy, w, h, v);
        
        x_out = x - dt*((2.0/9.0)*firstU + (3.0/9.0)*midU + (4.0/9.0)*lastU);
        y_out = y - dt*((2.0/9.0)*firstV + (3.0/9.0)*midV + (4.0/9.0)*lastV);
    

end
