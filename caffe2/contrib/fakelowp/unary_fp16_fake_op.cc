#include "unary_fp16_fake_op.h"
#include <fbgemm/FbgemmConvert.h>

#include "caffe2/utils/eigen_utils.h"

namespace caffe2 {

namespace {
auto sig_lut = std::vector<at::Half>{
    0.0000e+00f, 0.0000e+00f, 0.0000e+00f, 0.0000e+00f, 0.0000e+00f,
    0.0000e+00f, 0.0000e+00f, 5.9605e-08f, 5.9605e-08f, 5.9605e-08f,
    5.9605e-08f, 5.9605e-08f, 5.9605e-08f, 5.9605e-08f, 5.9605e-08f,
    5.9605e-08f, 5.9605e-08f, 1.1921e-07f, 1.1921e-07f, 1.1921e-07f,
    1.1921e-07f, 1.7881e-07f, 1.7881e-07f, 1.7881e-07f, 2.3842e-07f,
    2.3842e-07f, 2.3842e-07f, 2.9802e-07f, 2.9802e-07f, 3.5763e-07f,
    4.1723e-07f, 4.7684e-07f, 4.7684e-07f, 5.3644e-07f, 6.5565e-07f,
    7.1526e-07f, 7.7486e-07f, 8.9407e-07f, 9.5367e-07f, 1.0729e-06f,
    1.1921e-06f, 1.3709e-06f, 1.4901e-06f, 1.6689e-06f, 1.8477e-06f,
    2.0862e-06f, 2.3246e-06f, 2.6226e-06f, 2.9206e-06f, 3.2187e-06f,
    3.6359e-06f, 4.0531e-06f, 4.4703e-06f, 5.0068e-06f, 5.6028e-06f,
    6.2585e-06f, 6.9737e-06f, 7.7486e-06f, 8.6427e-06f, 9.6560e-06f,
    1.0788e-05f, 1.2040e-05f, 1.3411e-05f, 1.4961e-05f, 1.6689e-05f,
    1.8597e-05f, 2.0742e-05f, 2.3186e-05f, 2.5868e-05f, 2.8849e-05f,
    3.2187e-05f, 3.5882e-05f, 4.0054e-05f, 4.4644e-05f, 4.9829e-05f,
    5.5552e-05f, 6.1989e-05f, 6.9141e-05f, 7.7128e-05f, 8.6069e-05f,
    9.6023e-05f, 1.0711e-04f, 1.1951e-04f, 1.3328e-04f, 1.4865e-04f,
    1.6594e-04f, 1.8501e-04f, 2.0647e-04f, 2.3031e-04f, 2.5702e-04f,
    2.8658e-04f, 3.1972e-04f, 3.5667e-04f, 3.9792e-04f, 4.4370e-04f,
    4.9496e-04f, 5.5218e-04f, 6.1607e-04f, 6.8712e-04f, 7.6675e-04f,
    8.5497e-04f, 9.5367e-04f, 1.0643e-03f, 1.1864e-03f, 1.3237e-03f,
    1.4763e-03f, 1.6470e-03f, 1.8368e-03f, 2.0485e-03f, 2.2850e-03f,
    2.5482e-03f, 2.8419e-03f, 3.1700e-03f, 3.5343e-03f, 3.9406e-03f,
    4.3945e-03f, 4.9019e-03f, 5.4626e-03f, 6.0921e-03f, 6.7902e-03f,
    7.5722e-03f, 8.4381e-03f, 9.4070e-03f, 1.0483e-02f, 1.1681e-02f,
    1.3008e-02f, 1.4488e-02f, 1.6144e-02f, 1.7975e-02f, 2.0004e-02f,
    2.2263e-02f, 2.4780e-02f, 2.7557e-02f, 3.0655e-02f, 3.4058e-02f,
    3.7872e-02f, 4.2053e-02f, 4.6692e-02f, 5.1819e-02f, 5.7434e-02f,
    6.3660e-02f, 7.0496e-02f, 7.8003e-02f, 8.6243e-02f, 9.5276e-02f,
    1.0516e-01f, 1.1591e-01f, 1.2756e-01f, 1.4026e-01f, 1.5393e-01f,
    1.6882e-01f, 1.8469e-01f, 2.0178e-01f, 2.1997e-01f, 2.3926e-01f,
    2.5977e-01f, 2.8125e-01f, 3.0396e-01f, 3.2764e-01f, 3.5205e-01f,
    3.7744e-01f, 4.0356e-01f, 4.3018e-01f, 4.5703e-01f, 4.8438e-01f,
    5.1172e-01f, 5.3906e-01f, 5.6592e-01f, 5.9277e-01f, 6.1865e-01f,
    6.4404e-01f, 6.6895e-01f, 6.9287e-01f, 7.1533e-01f, 7.3730e-01f,
    7.5781e-01f, 7.7734e-01f, 7.9590e-01f, 8.1299e-01f, 8.2910e-01f,
    8.4375e-01f, 8.5791e-01f, 8.7061e-01f, 8.8232e-01f, 8.9355e-01f,
    9.0332e-01f, 9.1260e-01f, 9.2090e-01f, 9.2822e-01f, 9.3555e-01f,
    9.4189e-01f, 9.4727e-01f, 9.5264e-01f, 9.5752e-01f, 9.6143e-01f,
    9.6533e-01f, 9.6875e-01f, 9.7217e-01f, 9.7461e-01f, 9.7754e-01f,
    9.7949e-01f, 9.8193e-01f, 9.8340e-01f, 9.8535e-01f, 9.8682e-01f,
    9.8828e-01f, 9.8926e-01f, 9.9023e-01f, 9.9121e-01f, 9.9219e-01f,
    9.9316e-01f, 9.9365e-01f, 9.9463e-01f, 9.9512e-01f, 9.9561e-01f,
    9.9609e-01f, 9.9658e-01f, 9.9658e-01f, 9.9707e-01f, 9.9756e-01f,
    9.9756e-01f, 9.9805e-01f, 9.9805e-01f, 9.9854e-01f, 9.9854e-01f,
    9.9854e-01f, 9.9902e-01f, 9.9902e-01f, 9.9902e-01f, 9.9902e-01f,
    9.9902e-01f, 9.9951e-01f, 9.9951e-01f, 9.9951e-01f, 9.9951e-01f,
    9.9951e-01f, 9.9951e-01f, 9.9951e-01f, 9.9951e-01f, 9.9951e-01f,
    9.9951e-01f, 1.0000e+00f, 1.0000e+00f, 1.0000e+00f, 1.0000e+00f,
    1.0000e+00f, 1.0000e+00f, 1.0000e+00f, 1.0000e+00f, 1.0000e+00f,
    1.0000e+00f, 1.0000e+00f, 1.0000e+00f, 1.0000e+00f, 1.0000e+00f,
    1.0000e+00f, 1.0000e+00f};

at::Half CalcSigmoidByLUT(at::Half x) {
  at::Half a = -18.0;
  at::Half b = 10.0;
  int nBins = 256;

  at::Half delta = (b - a) / (at::Half)nBins;
  at::Half one_over_delta = 1 / delta;
  at::Half a_one_over_delta = a * one_over_delta;

  // Clamp the input in the range of a to b
  if (x < a) {
    x = a;
  }

  if (x > b) {
    x = b;
  }

  at::Half bin_calc = std::fma(x, one_over_delta, -a_one_over_delta);

  uint32_t bin = bin_calc < 0 ? 0 : (uint32_t)floor(bin_calc);
  // Clamp bin to SIGMOID_KNOT_LUT_SIZE-2, to have valid LUT access i.e. b+1 =
  // 255 (for LUT size of 256)
  if (bin > 254) {
    bin = 254;
  }
  // Use MAC bin_x = a + delta * at::Half(bin);
  at::Half bin_x = std::fma(delta, at::Half(bin), a);

  at::Half p = at::Half(x - bin_x) * one_over_delta;

  at::Half res1 = sig_lut[bin + 1] * p;
  // Use MAC res2 = (1 - p) * lut[bin] = -p * lut[bin] + lut[bin]
  at::Half res2 = std::fma(-p, sig_lut[bin], sig_lut[bin]);

  return at::Half(res1 + res2);
}

OpSchema::Cost CostInferenceForRelu(
    const OperatorDef& def,
    const vector<TensorShape>& in) {
  struct OpSchema::Cost cost = PointwiseCostInference<0>(def, in);
  cost.params_bytes = 0;
  return cost;
}

const int TANH_LINEAR_MAX_VALUE = 10048;
const int TANH_ASYMPTOTE_MIN_VALUE = 17538;

static float tanh_lut[] = {
    at::Half(0.02831274f), at::Half(0.02928850f), at::Half(0.03026419f),
    at::Half(0.03123983f), at::Half(0.03319093f), at::Half(0.03514177f),
    at::Half(0.03709235f), at::Half(0.03904264f), at::Half(0.04099264f),
    at::Half(0.04294232f), at::Half(0.04489168f), at::Half(0.04684070f),
    at::Half(0.04878936f), at::Half(0.05073764f), at::Half(0.05268555f),
    at::Half(0.05463305f), at::Half(0.05658013f), at::Half(0.05852679f),
    at::Half(0.06047300f), at::Half(0.06241875f), at::Half(0.06630881f),
    at::Half(0.07019686f), at::Half(0.07408277f), at::Half(0.07796644f),
    at::Half(0.08184774f), at::Half(0.08572657f), at::Half(0.08960279f),
    at::Half(0.09347630f), at::Half(0.09734699f), at::Half(0.10121473f),
    at::Half(0.10507942f), at::Half(0.10894093f), at::Half(0.11279916f),
    at::Half(0.11665399f), at::Half(0.12050531f), at::Half(0.12435300f),
    at::Half(0.13203707f), at::Half(0.13970530f), at::Half(0.14735681f),
    at::Half(0.15499073f), at::Half(0.16260618f), at::Half(0.17020231f),
    at::Half(0.17777826f), at::Half(0.18533320f), at::Half(0.19286629f),
    at::Half(0.20037672f), at::Half(0.20786367f), at::Half(0.21532634f),
    at::Half(0.22276395f), at::Half(0.23017571f), at::Half(0.23756087f),
    at::Half(0.24491866f), at::Half(0.25954921f), at::Half(0.27406159f),
    at::Half(0.28845021f), at::Half(0.30270973f), at::Half(0.31683500f),
    at::Half(0.33082112f), at::Half(0.34466340f), at::Half(0.35835740f),
    at::Half(0.37189891f), at::Half(0.38528397f), at::Half(0.39850884f),
    at::Half(0.41157006f), at::Half(0.42446437f), at::Half(0.43718879f),
    at::Half(0.44974055f), at::Half(0.46211716f), at::Half(0.48633602f),
    at::Half(0.50982997f), at::Half(0.53258729f), at::Half(0.55459972f),
    at::Half(0.57586239f), at::Half(0.59637356f), at::Half(0.61613443f),
    at::Half(0.63514895f), at::Half(0.65342359f), at::Half(0.67096707f),
    at::Half(0.68779021f), at::Half(0.70390560f), at::Half(0.71932750f),
    at::Half(0.73407152f), at::Half(0.74815447f), at::Half(0.76159416f),
    at::Half(0.78661881f), at::Half(0.80930107f), at::Half(0.82980191f),
    at::Half(0.84828364f), at::Half(0.86490662f), at::Half(0.87982670f),
    at::Half(0.89319334f), at::Half(0.90514825f), at::Half(0.91582454f),
    at::Half(0.92534623f), at::Half(0.93382804f), at::Half(0.94137554f),
    at::Half(0.94808529f), at::Half(0.95404526f), at::Half(0.95933529f),
    at::Half(0.96402758f), at::Half(0.97187275f), at::Half(0.97802611f),
    at::Half(0.98284503f), at::Half(0.98661430f), at::Half(0.98955975f),
    at::Half(0.99185972f), at::Half(0.99365463f), at::Half(0.99505475f),
    at::Half(0.99614653f), at::Half(0.99699764f), at::Half(0.99766098f),
    at::Half(0.99817790f), at::Half(0.99858066f), at::Half(0.99889444f),
    at::Half(0.99913889f), at::Half(0.99932930f), at::Half(0.99959315f),
    at::Half(0.99975321f), at::Half(0.99985031f), at::Half(0.99990920f),
    at::Half(0.99994493f), at::Half(0.99996660f), at::Half(0.99997974f),
    at::Half(0.99998771f), at::Half(0.99999255f), at::Half(0.99999548f),
    at::Half(0.99999726f), at::Half(0.99999834f)};

static float tanh_error_lut[] = {
    at::Half(0.00001525f), at::Half(0.00001525f), at::Half(0.00001524f),
    at::Half(0.00003049f), at::Half(0.00003048f), at::Half(0.00003048f),
    at::Half(0.00003047f), at::Half(0.00003047f), at::Half(0.00003046f),
    at::Half(0.00003046f), at::Half(0.00003045f), at::Half(0.00003045f),
    at::Half(0.00003044f), at::Half(0.00003044f), at::Half(0.00003043f),
    at::Half(0.00003042f), at::Half(0.00003042f), at::Half(0.00003041f),
    at::Half(0.00003040f), at::Half(0.00006078f), at::Half(0.00006075f),
    at::Half(0.00006072f), at::Half(0.00006068f), at::Half(0.00006065f),
    at::Half(0.00006061f), at::Half(0.00006057f), at::Half(0.00006052f),
    at::Half(0.00006048f), at::Half(0.00006043f), at::Half(0.00006039f),
    at::Half(0.00006034f), at::Half(0.00006028f), at::Half(0.00006023f),
    at::Half(0.00006018f), at::Half(0.00006012f), at::Half(0.00012006f),
    at::Half(0.00011982f), at::Half(0.00011955f), at::Half(0.00011928f),
    at::Half(0.00011899f), at::Half(0.00011869f), at::Half(0.00011837f),
    at::Half(0.00011805f), at::Half(0.00011770f), at::Half(0.00011735f),
    at::Half(0.00011698f), at::Half(0.00011660f), at::Half(0.00011621f),
    at::Half(0.00011581f), at::Half(0.00011539f), at::Half(0.00011497f),
    at::Half(0.00022860f), at::Half(0.00022676f), at::Half(0.00022482f),
    at::Half(0.00022281f), at::Half(0.00022071f), at::Half(0.00021853f),
    at::Half(0.00021629f), at::Half(0.00021397f), at::Half(0.00021159f),
    at::Half(0.00020914f), at::Half(0.00020664f), at::Half(0.00020408f),
    at::Half(0.00020147f), at::Half(0.00019882f), at::Half(0.00019612f),
    at::Half(0.00019338f), at::Half(0.00037842f), at::Half(0.00036709f),
    at::Half(0.00035558f), at::Half(0.00034394f), at::Half(0.00033223f),
    at::Half(0.00032049f), at::Half(0.00030876f), at::Half(0.00029710f),
    at::Half(0.00028554f), at::Half(0.00027412f), at::Half(0.00026286f),
    at::Half(0.00025180f), at::Half(0.00024097f), at::Half(0.00023038f),
    at::Half(0.00022005f), at::Half(0.00021000f), at::Half(0.00039101f),
    at::Half(0.00035441f), at::Half(0.00032033f), at::Half(0.00028878f),
    at::Half(0.00025973f), at::Half(0.00023313f), at::Half(0.00020885f),
    at::Half(0.00018680f), at::Half(0.00016682f), at::Half(0.00014878f),
    at::Half(0.00013253f), at::Half(0.00011793f), at::Half(0.00010484f),
    at::Half(0.00009312f), at::Half(0.00008266f), at::Half(0.00007332f),
    at::Half(0.00012258f), at::Half(0.00009615f), at::Half(0.00007530f),
    at::Half(0.00005889f), at::Half(0.00004602f), at::Half(0.00003594f),
    at::Half(0.00002805f), at::Half(0.00002188f), at::Half(0.00001706f),
    at::Half(0.00001330f), at::Half(0.00001036f), at::Half(0.00000808f),
    at::Half(0.00000629f), at::Half(0.00000490f), at::Half(0.00000382f),
    at::Half(0.00000298f), at::Half(0.00000000f), at::Half(0.00000000f),
    at::Half(0.00000000f), at::Half(0.00000000f), at::Half(0.00000000f),
    at::Half(0.00000000f), at::Half(0.00000000f), at::Half(0.00000000f),
    at::Half(0.00000000f), at::Half(0.00000000f), at::Half(0.00000000f),
    at::Half(0.00000000f), at::Half(0.00000000f)};

at::Half CalcTanhByLUT(at::Half input) {
  uint16_t InputInU16_temp;
  uint16_t InputInU16;
  int mask = 0x7FFF;
  uint16_t sign_bit;
  float unit = 1.0;
  float index;
  at::Half err_f16;
  at::Half output = at::Half(0.0f);

  /* Extracting bits 9-15 of f16 input to get the LUT index */
  InputInU16_temp = (*((uint16_t*)&input));
  sign_bit = InputInU16_temp & 0x8000;
  InputInU16 = InputInU16_temp & mask; // positive number
  if (InputInU16 < TANH_LINEAR_MAX_VALUE) {
    output = input;
  } else if (InputInU16 >= TANH_ASYMPTOTE_MIN_VALUE) {
    output = unit;
  } else {
    index = ((InputInU16 - TANH_LINEAR_MAX_VALUE) % 64);
    err_f16 =
        at::Half(tanh_error_lut[(InputInU16 - TANH_LINEAR_MAX_VALUE) / 64]);
    output = at::Half(tanh_lut[(InputInU16 - TANH_LINEAR_MAX_VALUE) / 64]);

    output = at::Half(std::fma(err_f16, index, output));
  }
  uint16_t outputInU16_temp = (*((uint16_t*)&output)) | sign_bit;
  output = (*((at::Half*)&outputInU16_temp));
  return output;
}

at::Half CalcTanhByPolynomial(at::Half input) {
  static const at::Half aCoefficient[64] = {
      at::Half(-0.423340f), at::Half(-0.352783f), at::Half(-0.411377f),
      at::Half(-0.284424f), at::Half(-0.335938f), at::Half(-0.333740f),
      at::Half(-0.333252f), at::Half(-0.332275f), at::Half(-0.333252f),
      at::Half(-0.333252f), at::Half(-0.333252f), at::Half(-0.333252f),
      at::Half(-0.333252f), at::Half(-0.333252f), at::Half(-0.333252f),
      at::Half(-0.333252f), at::Half(-0.333008f), at::Half(-0.333008f),
      at::Half(-0.332764f), at::Half(-0.332275f), at::Half(-0.331055f),
      at::Half(-0.329346f), at::Half(-0.325195f), at::Half(-0.317383f),
      at::Half(-0.301758f), at::Half(-0.273438f), at::Half(-0.219360f),
      at::Half(-0.136108f), at::Half(-0.018677f), at::Half(0.080872f),
      at::Half(0.107056f),  at::Half(0.063110f),  at::Half(0.017731f),
      at::Half(0.002533f),  at::Half(0.000147f),  at::Half(0.000003f),
      at::Half(0.000000f),  at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(0.000000f),  at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(0.000000f),  at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(0.000000f),  at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(0.000000f),  at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(0.000000f),  at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(0.000000f),  at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(0.000000f),  at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(0.000000f),  at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(0.000000f)};
  static const at::Half bCoefficient[64] = {
      at::Half(0.000004f),  at::Half(0.000002f),  at::Half(0.000017f),
      at::Half(-0.000016f), at::Half(0.000001f),  at::Half(0.000000f),
      at::Half(0.000000f),  at::Half(-0.000001f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000001f), at::Half(-0.000002f), at::Half(-0.000007f),
      at::Half(-0.000020f), at::Half(-0.000054f), at::Half(-0.000158f),
      at::Half(-0.000433f), at::Half(-0.001253f), at::Half(-0.003410f),
      at::Half(-0.009712f), at::Half(-0.025681f), at::Half(-0.068665f),
      at::Half(-0.162354f), at::Half(-0.346680f), at::Half(-0.566406f),
      at::Half(-0.640137f), at::Half(-0.439941f), at::Half(-0.161255f),
      at::Half(-0.030548f), at::Half(-0.002459f), at::Half(-0.000061f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f)};
  static const at::Half cCoefficient[64] = {
      at::Half(1.000000f), at::Half(1.000000f), at::Half(1.000000f),
      at::Half(1.000000f), at::Half(1.000000f), at::Half(1.000000f),
      at::Half(1.000000f), at::Half(1.000000f), at::Half(1.000000f),
      at::Half(1.000000f), at::Half(1.000000f), at::Half(1.000000f),
      at::Half(1.000000f), at::Half(1.000000f), at::Half(1.000000f),
      at::Half(1.000000f), at::Half(1.000000f), at::Half(1.000000f),
      at::Half(1.000000f), at::Half(1.000000f), at::Half(1.000000f),
      at::Half(1.000000f), at::Half(1.000000f), at::Half(1.000000f),
      at::Half(1.000977f), at::Half(1.003906f), at::Half(1.014648f),
      at::Half(1.050781f), at::Half(1.147461f), at::Half(1.309570f),
      at::Half(1.378906f), at::Half(1.073242f), at::Half(0.500488f),
      at::Half(0.124329f), at::Half(0.013718f), at::Half(0.000464f),
      at::Half(0.000004f), at::Half(0.000000f), at::Half(0.000000f),
      at::Half(0.000000f), at::Half(0.000000f), at::Half(0.000000f),
      at::Half(0.000000f), at::Half(0.000000f), at::Half(0.000000f),
      at::Half(0.000000f), at::Half(0.000000f), at::Half(0.000000f),
      at::Half(0.000000f), at::Half(0.000000f), at::Half(0.000000f),
      at::Half(0.000000f), at::Half(0.000000f), at::Half(0.000000f),
      at::Half(0.000000f), at::Half(0.000000f), at::Half(0.000000f),
      at::Half(0.000000f), at::Half(0.000000f), at::Half(0.000000f),
      at::Half(0.000000f), at::Half(0.000000f)};
  static const at::Half dCoefficient[64] = {
      at::Half(-0.000000f), at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(-0.000000f), at::Half(0.000000f),  at::Half(0.000000f),
      at::Half(0.000000f),  at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000000f), at::Half(-0.000000f),
      at::Half(-0.000000f), at::Half(-0.000001f), at::Half(-0.000008f),
      at::Half(-0.000045f), at::Half(-0.000237f), at::Half(-0.001252f),
      at::Half(-0.005722f), at::Half(-0.022766f), at::Half(-0.062866f),
      at::Half(-0.084229f), at::Half(0.071167f),  at::Half(0.466064f),
      at::Half(0.828125f),  at::Half(0.974121f),  at::Half(0.998535f),
      at::Half(0.999512f),  at::Half(1.000000f),  at::Half(1.000000f),
      at::Half(1.000000f),  at::Half(1.000000f),  at::Half(1.000000f),
      at::Half(1.000000f),  at::Half(1.000000f),  at::Half(1.000000f),
      at::Half(1.000000f),  at::Half(1.000000f),  at::Half(1.000000f),
      at::Half(1.000000f),  at::Half(1.000000f),  at::Half(1.000000f),
      at::Half(1.000000f),  at::Half(1.000000f),  at::Half(1.000000f),
      at::Half(1.000000f),  at::Half(1.000000f),  at::Half(1.000000f),
      at::Half(1.000000f),  at::Half(1.000000f),  at::Half(1.000000f),
      at::Half(1.000000f),  at::Half(1.000000f),  at::Half(1.000000f),
      at::Half(1.000000f)};

  int16_t temp = *((unsigned short*)(&input));
  int16_t index = ((temp & 0x7E00) >> 9); // extract bits 9..14

  // Because tanh is anti-symmetric, we can perform the operation for abs(t_2)
  // and then multiply the result by -1 in case the number is negative.
  at::Half absInput = (input < 0) ? (input * at::Half(-1)) : input;

  at::Half a = aCoefficient[index];
  at::Half b = bCoefficient[index];
  at::Half c = cCoefficient[index];
  at::Half d = dCoefficient[index];

  b = b + a * absInput;
  c = c + b * absInput;
  at::Half tanhResult = d + c * absInput;
  tanhResult =
      (input < 0) ? tanhResult * -1 : tanhResult; // tanh is anti-symmetric

  return tanhResult;
}

struct SigmoidEmulatorFunctor {
  bool operator()(
      const int N,
      const float* X,
      float* Y,
      CPUContext* /* unused */) const {
    for (int i = 0; i < N; i++) {
      Y[i] = CalcSigmoidByLUT((at::Half)X[i]);
    }
    return true;
  }
};

struct TanhEmulatorFunctor {
  bool operator()(
      const int N,
      const float* X,
      float* Y,
      CPUContext* /* unused */) const {
    for (int i = 0; i < N; i++) {
      Y[i] = CalcTanhByLUT((at::Half)X[i]);
    }
    return true;
  }
};

} // namespace

REGISTER_CPU_OPERATOR(
    ReluFakeFp16,
    UnaryElementwiseOp<
        TensorTypes<float>,
        CPUContext,
        ReluFakeFp16Functor<CPUContext>>);

// Input: X, output: Y
OPERATOR_SCHEMA(ReluFakeFp16)
    .NumInputs(1)
    .NumOutputs(1)
    .AllowInplace({{0, 0}})
    .CostInferenceFunction(CostInferenceForRelu)
    .IdenticalTypeAndShape()
    .SetDoc(R"DOC(
Applies rectified linear unit operation to the input data element-wise. The Relu operation takes one input $X$, produces one output $Y$, and is defined as:

$$Y = max(0,X)$$

The input of this operator is converted to fp16 precision. And since the ReLU
op doesn't have any arithmetics, there is no need to convert the output.

<details>

<summary> <b>Example</b> </summary>

**Code**

```
workspace.ResetWorkspace()

op = core.CreateOperator(
  "ReluFakeFp16",
  ["X"],
  ["Y"]
  )

workspace.FeedBlob("X", np.random.randn(4, 4).astype(np.float32)) // NCHW
print("X:\n", workspace.FetchBlob("X"), "\n")

workspace.RunOperatorOnce(op)
print("Y:\n", workspace.FetchBlob("Y"))

```

**Result**

```

X:
 [[-1.4655551   0.64575136  0.7921748   0.4150579 ]
 [ 0.41085166 -0.2837964   0.9881425  -1.9300346 ]
 [ 0.39705405  0.44639114  0.9940703   0.2926532 ]
 [-0.6726489   0.01330667  1.101319    0.33858967]]

Y:
 [[0.         0.64575136 0.7921748  0.4150579 ]
 [0.41085166 0.         0.9881425  0.        ]
 [0.39705405 0.44639114 0.9940703  0.2926532 ]
 [0.         0.01330667 1.101319   0.33858967]]

```

</details>


)DOC")
    .Input(0, "X", "1D input tensor")
    .Output(0, "Y", "1D output tensor with same shape as input")
    .InheritOnnxSchema();

REGISTER_CPU_OPERATOR(
    SigmoidFakeFp16NNPI,
    UnaryElementwiseOp<TensorTypes<float>, CPUContext, SigmoidEmulatorFunctor>);

REGISTER_CPU_OPERATOR(
    SigmoidFakeFp16,
    UnaryElementwiseOp<
        TensorTypes<float>,
        CPUContext,
        SigmoidFakeIdealFp16Functor>);

// Input: X, output: Y
OPERATOR_SCHEMA(SigmoidFakeFp16)
    .NumInputs(1)
    .NumOutputs(1)
    .AllowInplace({{0, 0}})
    .IdenticalTypeAndShape()
    .SetDoc(R"DOC(
Apply the Sigmoid function element-wise to the input tensor. This is often used
as a non-linear activation function in a neural network. The sigmoid function is
defined as:

$$Sigmoid(x) = \frac{1}{1+\exp(-x)}$$

The input and output of this operator are converted to fp16 precision.

<details>

<summary> <b>Example</b> </summary>

**Code**

```

workspace.ResetWorkspace()

op = core.CreateOperator(
    "SigmoidFakeFp16",
    ["X"],
    ["Y"]
)

workspace.FeedBlob("X", np.random.randn(5).astype(np.float32))
print("input:", workspace.FetchBlob("X"))
workspace.RunOperatorOnce(op)
print("sigmoid:", workspace.FetchBlob("Y"))

```

**Result**

```

input: [ 1.5744036   0.31632107  1.7842269   1.4450722  -2.1726978 ]
sigmoid: [0.8284105  0.57842743 0.85621804 0.80923885 0.10222916]

```

</details>


)DOC")
    .Input(0, "X", "*(type: Tensor`<float>`)* Input tensor.")
    .Output(0, "Y", "*(type: Tensor`<float>`)* Output tensor.")
    .InheritOnnxSchema();

REGISTER_CPU_OPERATOR(
    SqrFakeFp16,
    UnaryElementwiseOp<
        TensorTypes<float>,
        CPUContext,
        SqrFakeFp16Functor<CPUContext>>);

OPERATOR_SCHEMA(SqrFakeFp16)
    .NumInputs(1)
    .NumOutputs(1)
    .AllowInplace({{0, 0}})
    .IdenticalTypeAndShape()
    .SetDoc(R"DOC(
Performs element-wise squaring ($x^2$) of input tensor. Inputs are converted
to fp16 before the operation, computation is in fp32, and the result is
also converted to fp16.


<details>

<summary> <b>Example</b> </summary>

**Code**

```

workspace.ResetWorkspace()

op = core.CreateOperator(
    "SqrFakeFp16",
    ["X"],
    ["Y"],
)

workspace.FeedBlob("X", (np.random.randint(10, size=(3,3))).astype(np.float32))
print("X:", workspace.FetchBlob("X"))
workspace.RunOperatorOnce(op)
print("Y:", workspace.FetchBlob("Y"))

```

**Result**

```

X:
[[4. 6. 2.]
 [0. 1. 6.]
 [9. 2. 7.]]
Y:
[[16. 36.  4.]
 [ 0.  1. 36.]
 [81.  4. 49.]]

```

</details>

    )DOC")
    .Input(0, "X", "*(type: Tensor`<float>`)* Input data tensor.")
    .Output(0, "Y", "*(type: Tensor`<float>`)* Output tensor.");

REGISTER_CPU_OPERATOR(
    TanhFakeFp16NNPI,
    UnaryElementwiseOp<TensorTypes<float>, CPUContext, TanhEmulatorFunctor>);

REGISTER_CPU_OPERATOR(
    TanhFakeFp16,
    UnaryElementwiseOp<
        TensorTypes<float>,
        CPUContext,
        TanhFakeIdealFp16Functor>);

OPERATOR_SCHEMA(TanhFakeFp16)
    .NumInputs(1)
    .NumOutputs(1)
    .AllowInplace({{0, 0}})
    .IdenticalTypeAndShape()
    .SetDoc(R"DOC(
Calculates the hyperbolic tangent of the given input tensor element-wise. This
operation can be done in an in-place fashion too, by providing the same input
and output blobs.

The input and output of this operator are converted to fp16 precision.

<details>

<summary> <b>Example</b> </summary>

**Code**

```

workspace.ResetWorkspace()

op = core.CreateOperator(
    "TanhFakeFp16",
    ["X"],
    ["X"],
)

workspace.FeedBlob("X", np.random.randn(3, 3).astype(np.float32))
print("X:\n", workspace.FetchBlob("X"), "\n")

workspace.RunOperatorOnce(op)
print("X:\n", workspace.FetchBlob("X"))

```

**Result**

```

X:
 [[ 2.032603   -2.3556721  -0.14955314]
 [ 0.39309832 -1.1020128  -0.92951244]
 [-0.62815386  0.21342885  1.4002231 ]]

X:
 [[ 0.9662601  -0.982175   -0.14844811]
 [ 0.3740282  -0.8012209  -0.73036647]
 [-0.55677974  0.21024609  0.8853999 ]]

```

</details>

)DOC")
    .Input(0, "input", "1-D input tensor")
    .Output(
        0,
        "output",
        "The hyperbolic tangent values of the input tensor, computed "
        "element-wise")
    .InheritOnnxSchema();

} // namespace caffe2
