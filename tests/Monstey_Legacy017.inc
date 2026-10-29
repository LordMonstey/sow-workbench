// Exact scalar helpers copied privately from frozen ordinary core 4ccd774e...
using Monstey_Q=std::array<float,4>;
using Monstey_V=std::array<float,3>;
struct Monstey_Pose { Monstey_V position_cm; Monstey_Q quaternion_xyzw; };
Monstey_V Monstey_APRotate(const Monstey_Q &q, const Monstey_V &t)
{
    const float z = t[2], y = t[1], yx = q[0] * y, qy = q[1], qw = q[3], yy = y * qy;
    const float a = (float)((float)(t[0] * qw) + (float)(qy * z)) - (float)(q[2] * y);
    const float b = (float)((float)(y * qw) - (float)(q[0] * z)) + (float)(q[2] * t[0]);
    const float c = (float)(yx + (float)(qw * z)) - (float)(t[0] * qy);
    const float ny = -qy, nx = -q[0], nz = -q[2];
    const float d = (float)((float)-(float)(q[0] * t[0]) - yy) - (float)(q[2] * z);
    return {(float)((float)((float)(nx * d) + (float)(qw * a)) + (float)(nz * b)) - (float)(ny * c),
            (float)((float)((float)(ny * d) - (float)(nz * a)) + (float)(qw * b)) + (float)(nx * c),
            (float)((float)((float)(nz * d) + (float)(ny * a)) - (float)(nx * b)) + (float)(qw * c)};
}
Monstey_Q Monstey_APComposeQ(const Monstey_Q &p, const Monstey_Q &c)
{
    return {(float)((float)((float)(c[0] * p[3]) + (float)(p[0] * c[3])) + (float)(p[1] * c[2])) -
                (float)(p[2] * c[1]),
            (float)((float)((float)(c[1] * p[3]) - (float)(c[2] * p[0])) + (float)(p[1] * c[3])) +
                (float)(p[2] * c[0]),
            (float)((float)((float)(c[2] * p[3]) + (float)(c[1] * p[0])) - (float)(p[1] * c[0])) +
                (float)(p[2] * c[3]),
            (float)((float)((float)(p[3] * c[3]) - (float)(c[0] * p[0])) - (float)(c[1] * p[1])) -
                (float)(p[2] * c[2])};
}
Monstey_Pose Monstey_APCompose(const Monstey_Pose &parent, const Monstey_Pose &child)
{
    auto result = child;
    result.position_cm = Monstey_APRotate(parent.quaternion_xyzw, child.position_cm);
    for (unsigned k = 0; k < 3; ++k)
        result.position_cm[k] += parent.position_cm[k];
    result.quaternion_xyzw = Monstey_APComposeQ(parent.quaternion_xyzw, child.quaternion_xyzw);
    return result;
}
