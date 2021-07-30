#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <new.h>

#include <algorithm>

#include "xoshiro_dbj.h"

// for the simple for's
#define FOR(C_, N_) for (int C_ = 0; C_ < N_; ++C_)

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
    typedef long double Real;
    typedef float ImageReal;

    static inline Real random(Real lo, Real hi)
    {
        static auto once_ = []()
        {
#ifndef NDEBUG
            printf("\ncalling xoshiro_dbj_ctor()");
#endif
            xoshiro_dbj_ctor();
            return 1;
        }();
        (void)once_;

        assert(lo <= hi);
        static Real scale_ = Real(1) / Real(-1uLL);
        Real r = Real(xoshiro_dbj_next()) * scale_;
        return lo + r * (hi - lo);
    }

#define BITMAP_HEIGHT 640
#define BITMAP_WIDTH 480

    // I understand the aim, but ...
    // ImageReal (&image_buffer)[3][BITMAP_HEIGHT][BITMAP_WIDTH] =
    //     *new ImageReal[1][3][BITMAP_HEIGHT][BITMAP_WIDTH]{};
    // that was a mem leak, this bellow goes onto stack, it is simple, fast and perhaps dangerous
    ImageReal image_buffer[3][BITMAP_HEIGHT][BITMAP_WIDTH];
#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////////////
namespace
{

    struct TargetProperties
    {
        Real x;
        Real y;
        Real zoom;
    };

    constexpr TargetProperties targets[] = {
        {0, 0, 0},                          // 0 the empty set
        {-0.4, 0, 0.32},                    // 1 the full set
        {-1.25275, -0.343, 250},            // 2
        {-0.1592, -1.0317, 80.5},           // 3
        {-0.529854097, -0.667968575, 80.5}, // 4
        {-0.657560793, 0.467732884, 70.5},  // 5
        {-1.185768799, 0.302592593, 90.5},  // 6
        {0.443108035, 0.345012263, 4000},   // 7
        {-0.647663050, 0.380700836, 1275},  // 8
        {-0.0443594, -0.986749, 88.2},      // 9 steckles.com sample image
    };

    constexpr TargetProperties target = targets[3];

    struct Complex
    {
        Real a = 0;
        Real b = 0;
        Real c = 0;
        Real d = 0;

        explicit Complex() = default;
        constexpr explicit Complex(Real a, Real b, Real c, Real d) : a(a), b(b), c(c), d(d) {}

        constexpr Real mag2() const { return c * c + d * d; }
        Real dist2(Real cp, Real dp) const { return (c - cp) * (c - cp) + (d - dp) * (d - dp); }
    };

    constexpr int max_orbit_length = 50000;
    Complex orbit[max_orbit_length];
    int olen = 0;

    struct ImageCoord
    {
        int y;
        int x;
        constexpr explicit ImageCoord(int x, int y) : y(y), x(x) {}
        bool isOnScreen() const { return (0 <= y && y < BITMAP_HEIGHT && 0 <= x && x < BITMAP_WIDTH); }
    };

    ImageCoord Project(const Complex &s)
    {
        // Notice that the image is rotated 90 degrees compared to the original complex plane.
        // Negative real components (target.x) turn into upward displacements (small coord.y).
        return ImageCoord(
            int((s.b - target.y) * target.zoom * BITMAP_HEIGHT + (BITMAP_WIDTH / 2)),
            int((s.a - target.x) * target.zoom * BITMAP_HEIGHT + (BITMAP_HEIGHT / 2)));
    }

    constexpr Complex Unproject(ImageCoord p)
    {
        return Complex(
            0, 0,
            (p.y - (BITMAP_HEIGHT / 2)) / (target.zoom * BITMAP_HEIGHT) + target.x,
            (p.x - (BITMAP_WIDTH / 2)) / (target.zoom * BITMAP_HEIGHT) + target.y);
    }

    Complex RandomInRadiusAround(Real x, Real y, Real r)
    {
        const auto r2 = r * r;
        while (true)
        {
            Complex c = Complex(0, 0, random(-r, r), random(-r, r));
            if (c.mag2() < r2)
            {
                c.c += x;
                c.d += y;
                return c;
            }
        }
    }

    // This function is essentially gamma-correction at both ends of the range [0,1].
    // It applies the inverse of a sigmoid function to artificially increase the
    // brightness of small-but-not-zero values and decrease the brightness of
    // large-but-not-1 values, thus compressing the output into the middle of the range.
    // However, this still tends to make things too dark, so after that,
    // we multiply everything by two (saturating at 1.0).
    // Then, we multiply by 256 to get a final pixel value.
    //
    unsigned char GainCorrect(ImageReal x)
    {
        auto bend = [](ImageReal x)
        {
            assert(0 <= x && x <= 0.5);
            ImageReal y = pow(2 * x, 0.3219280948873623) / 2;
            assert(0 <= y && y <= 0.5);
            return y;
        };
        ImageReal gain_adjusted = (x <= 0.5) ? bend(x) : (1 - bend(1 - x));
        ImageReal brightened = gain_adjusted * 2; // This factor seems unexplained.
        int m = brightened * 256;
        return (m < 0) ? 0 : (m < 255) ? m
                                       : 255;
    }

    void OutputPPM(const char *fname, ImageReal max0, ImageReal max1, ImageReal max2)
    {
        FILE *fp = fopen(fname, "wb");
        assert(fp != nullptr);
        fprintf(fp, "P6\n%d %d\n255\n", BITMAP_WIDTH, BITMAP_HEIGHT);
        FOR(y, BITMAP_HEIGHT)
        {
            FOR(x, BITMAP_WIDTH)
            {
                unsigned char rgb[3] = {
                    GainCorrect(image_buffer[0][y][x] / max0),
                    GainCorrect(image_buffer[1][y][x] / max1),
                    GainCorrect(image_buffer[2][y][x] / max2),
                };
                fwrite(rgb, 1, 3, fp);
            }
        }
        fclose(fp);
    }

#ifdef OUTPUT_PGMS
    void OutputPGM(const char *fname, int c, ImageReal cmax)
    {
        FILE *fp = fopen(fname, "wb");
        assert(fp != nullptr);
        fprintf(fp, "P5\n%d %d\n255\n", BITMAP_WIDTH, BITMAP_HEIGHT);
        FOR(y, BITMAP_HEIGHT)
        {
            FOR(x, BITMAP_WIDTH)
            {
                unsigned char rgb[1] = {
                    GainCorrect(image_buffer[c][y][x] / cmax),
                };
                fwrite(rgb, 1, 1, fp);
            }
        }
        fclose(fp);
    }
#endif // OUTPUT_PGMS

    void DrawBuffer()
    {
        printf("DrawBuffer!\n");
        ImageReal max0 = 0.01; // avoid div-by-zero on a completely black image
        ImageReal max1 = 0.01;
        ImageReal max2 = 0.01;
        FOR(y, BITMAP_HEIGHT)
        {
            FOR(x, BITMAP_WIDTH)
            {
                max0 = std::max(max0, image_buffer[0][y][x]);
                max1 = std::max(max1, image_buffer[1][y][x]);
                max2 = std::max(max2, image_buffer[2][y][x]);
            }
        }

        OutputPPM("color.ppm", max0, max1, max2);
#ifdef OUTPUT_PGMS
        OutputPGM("red.pgm", 0, max0);
        OutputPGM("green.pgm", 1, max1);
        OutputPGM("blue.pgm", 2, max2);
#endif // OUTPUT_PGMS
    }

    constexpr Real computeEscapeMagnitude2()
    {
        Real m1 = Unproject(ImageCoord{0, 0}).mag2();
        Real m2 = Unproject(ImageCoord{BITMAP_WIDTH - 1, 0}).mag2();
        Real m3 = Unproject(ImageCoord{0, BITMAP_HEIGHT - 1}).mag2();
        Real m4 = Unproject(ImageCoord{BITMAP_WIDTH - 1, BITMAP_HEIGHT - 1}).mag2();
        Real result = 4.0;
        if (result < m1)
            result = m1;
        if (result < m2)
            result = m2;
        if (result < m3)
            result = m3;
        if (result < m4)
            result = m4;
        return result;
    }

    Real computeEscapeMagnitude()
    {
        return sqrt(computeEscapeMagnitude2());
    }

    bool Evaluate(const Complex &s, int it)
    {
        constexpr Real escape = computeEscapeMagnitude2();

        assert(it <= max_orbit_length);
        olen = 0;
        Real a = s.a;
        Real b = s.b;
        FOR(i, it)
        {
            Real t = a * a - b * b + s.c;
            b = 2 * a * b + s.d;
            a = t;
            if (a * a + b * b > escape)
            {
                return true;
            }
            orbit[olen++] = Complex(a, b, s.c, s.d);
        }
        return false;
    }

    void DrawOrbit(int c, const Complex *d_orbit, int len)
    {
        FOR(i, len)
        {
            ImageCoord p = Project(d_orbit[i]);
            if (p.isOnScreen())
            {
                image_buffer[c][p.y][p.x] += ImageReal(1);
                if (target.y == 0)
                {
                    // If the region being graphed is symmetrical, then make the image symmetrical too.
                    image_buffer[c][p.y][BITMAP_WIDTH - p.x - 1] += ImageReal(1);
                }
            }
        }
    }

    Real Contrib()
    {
        Real contrib = 0;
        FOR(i, olen)
        {
            ImageCoord p = Project(orbit[i]);
            if (p.isOnScreen())
            {
                contrib += 1;
            }
        }
        return contrib / Real(olen);
    }

    Real TransitionProbability(Real q1, Real q2, Real olen1, Real olen2)
    {
        return (olen1 * q2) / (olen2 * q1);
    }

    // Most mutations should be small,
    // but occasionally throw in a completely new point
    // to maintain ergodicity.
    //
    Complex Mutate(const Complex &c)
    {
        static int counter = 5;
        if (--counter != 0)
        {
            Complex n = c;
            Real r2 = (1.L / target.zoom) * 0.1;
            Real phi = random(0, 2 * M_PI);
            Real r = r2 * exp(-random(0, 6.9077));
            n.c += r * cos(phi);
            n.d += r * sin(phi);
            return n;
        }
        else
        {
            counter = 5;
            return RandomInRadiusAround(0, 0, computeEscapeMagnitude());
        }
    }

    /***********************************************************************/
    /*                                                                     */
    /* Recursively find a point whose orbit passes through the screen.     */
    /* Not required per-se, but it's a lot faster than just trying random  */
    /* samples over and over again until you find a good one, especially   */
    /* at higher zooms.                                                    */
    /*                                                                     */
    /***********************************************************************/

    bool FindInitialSample(Complex &c, Real x, Real y, Real rad, int f)
    {
        if (f > 500)
        {
            return false;
        }
        Complex seed;
        // int m = -1;
        Real closest = 1e20;
        FOR(i, 200)
        {
            Complex tmp = RandomInRadiusAround(x, y, rad);
            if (Evaluate(tmp, 50000))
            {
                if (Contrib() > 0)
                {
                    c = tmp;
                    return true;
                }
                FOR(q, olen)
                {
                    Real d = orbit[q].dist2(target.x, target.y);
                    if (d < closest)
                    {
                        // m = q;
                        closest = d;
                        seed = tmp;
                    }
                }
            }
        }
        return FindInitialSample(c, seed.c, seed.d, rad / 2, f + 1);
    }

#ifdef USE_CLASSIC_RENDERING
    static void RenderBuddhabrotClassic()
    {
        static constexpr int Iterations[] = {5000, 500, 50}; // TODO
        unsigned short m = 0;
        while (true)
        {
            Complex z = RandomInRadiusAround(0, 0, computeEscapeMagnitude());
            if (Evaluate(z, 50000))
            {
                for (int c = 0; c < 3; ++c)
                {
                    if (olen <= Iterations[c])
                    {
                        DrawOrbit(c, orbit, olen);
                        if (++m == 0)
                        {
                            DrawBuffer();
                        }
                    }
                }
            }
        }
    }
#endif // USE_CLASSIC_RENDERING

    /***********************************************************************/
    /*                                                                     */
    /* As many disparate parts of the Mandelbrot set can produce orbits    */
    /* that pass through the screen, an optimization is to run multiple    */
    /* copies of the algorithm from different starting values; that way    */
    /* you're less likely to miss important orbits early on.               */
    /*                                                                     */
    /***********************************************************************/

    constexpr int metro_threads = 30;

    Complex z_samples[metro_threads];
    Real c_samples[metro_threads];
    Real l[metro_threads][3];
    Real o[metro_threads][3];

    void BuildInitialSamplePoints()
    {
        FOR(i, metro_threads)
        {
            l[i][0] = o[i][0] = l[i][1] = o[i][1] = l[i][2] = o[i][2] = 1;

            Complex m;
            while (!FindInitialSample(m, 0, 0, 2, 0))
            {
                printf("Couldn't find seed %d!\n", i);
            }
            Evaluate(m, 50000);
            z_samples[i] = m;
            c_samples[i] = Contrib();
        }
    }

    void RenderBuddhabrot()
    {
        static constexpr int Iterations[] = {50000, 5000, 500};
        unsigned short m = 0;
        while (true)
        {
            FOR(ss, metro_threads)
            {
                Complex n_sample = Mutate(z_samples[ss]);
                if (Evaluate(n_sample, 50000))
                { // it does eventually escape
                    Real n_contrib = Contrib();
                    if (n_contrib != 0)
                    {
                        FOR(c, 3)
                        {
                            int it_length = Iterations[c];
                            if (olen <= it_length)
                            { // it escapes within it_length iterations
                                Real T1 = TransitionProbability(it_length, l[ss][c], olen, o[ss][c]);
                                Real T2 = TransitionProbability(l[ss][c], it_length, o[ss][c], olen);
                                Real alpha = (n_contrib * T1) / (c_samples[ss] * T2);
                                if (alpha > random(0, 1))
                                {
                                    c_samples[ss] = n_contrib;
                                    z_samples[ss] = n_sample;
                                    l[ss][c] = it_length;
                                    o[ss][c] = olen;
                                    DrawOrbit(c, orbit, olen);
                                    if (++m == 0)
                                    {
                                        DrawBuffer();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

} // anonymous namespace

int main(void)
{
    BuildInitialSamplePoints();
    RenderBuddhabrot();
}
