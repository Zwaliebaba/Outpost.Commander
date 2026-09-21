#include "pch.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{

namespace
{
/// Seed 42, stream 54 -- and those two numbers are not arbitrary. They are the seeding used by
/// PCG's own published `pcg32-demo`, so the first six entries below can be checked against a
/// reference this repository did not write. A table generated from a seed of our own choosing
/// would pin only that this build agrees with itself, which is a much weaker claim.
inline constexpr std::uint64_t PINNED_SEED = 42;
inline constexpr std::uint64_t PINNED_STREAM = 54;

/// The first thousand outputs, which is what M0.7 asks to be pinned. A change to the multiplier,
/// the seeding order, the shift amounts or the rotation fails here on the first entry it reaches.
inline constexpr std::array<std::uint32_t, 1000> PINNED_OUTPUT = {
  0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu, 0xbfc6a3adu, 0x812fff6du, 0xe61f305au, 0xf9384b90u,
  0x32db86feu, 0x1dc035f9u, 0xed786826u, 0x3822441du, 0x2ba113d7u, 0x1c5b818bu, 0xa233956au, 0x84da65e3u, 0xced67292u, 0xb2c0fe06u,
  0x91817130u, 0x55fe8917u, 0x47e92091u, 0x486af299u, 0xb1e882bbu, 0xc261e845u, 0x1a9b90f6u, 0x7964e884u, 0x5f36d7a4u, 0x1ee2052du,
  0x8519f5d5u, 0x293d4e4fu, 0x6d8f99fcu, 0xc3421509u, 0xa06cd7c6u, 0xe43064d3u, 0xe20f9bf0u, 0x401b50b7u, 0x8ef1ff3eu, 0xe357e2b2u,
  0xa4aeee37u, 0x2ad4426au, 0x9d11be94u, 0x7290c556u, 0x6e6f3787u, 0x050c2ee3u, 0x4fd73703u, 0xc6ff478bu, 0x4b1ca1e1u, 0x1654ea91u,
  0xcd08b2f2u, 0xf7ff3da8u, 0x78b1b8dau, 0xa100602cu, 0x9588585fu, 0xda028873u, 0x66b4f376u, 0x0e6b4b9au, 0x48167094u, 0x0d58cda0u,
  0x8f7238beu, 0xf79983f3u, 0x07e5d324u, 0xad78df52u, 0x1532ba74u, 0x1e4899e2u, 0x6c75df64u, 0x171ddc36u, 0xf2d8d74au, 0x24e6d907u,
  0x4780fd32u, 0x9adf408cu, 0xa25544cfu, 0xefc6a738u, 0x1aa23a54u, 0xc5a13ebbu, 0xf739edc9u, 0xc3a015fau, 0x3d5e1511u, 0xafc4d7fbu,
  0x3f413b5eu, 0x4660cb73u, 0x88fc773fu, 0xd6bed59cu, 0x63b3b54au, 0xd67d3ddeu, 0x23394f8bu, 0x13384b44u, 0xdd8b3abcu, 0xff59a21eu,
  0x3bb16d7eu, 0x6e01cb68u, 0xec34790eu, 0xb26c42adu, 0xd723c830u, 0xdfd10fcau, 0x7e362aa1u, 0x826ff323u, 0xcb8f63b5u, 0x9b3227e5u,
  0x9a61e339u, 0xe7de4cb5u, 0xa875db4au, 0x5d73f2b6u, 0x5d9838f5u, 0x22acc9e6u, 0xb1c6c855u, 0x68d0fa8bu, 0xa8548290u, 0x3cc78a9bu,
  0x363e5bf9u, 0x3449bb4au, 0x98fca88cu, 0x84a4d6e6u, 0xabca03ecu, 0x322e08efu, 0x5ac77ffdu, 0x83b8ee2cu, 0xd1dc12b3u, 0x265b1064u,
  0xb0a8fbe5u, 0x5b2cd7a3u, 0xd89722a5u, 0x1ae2d1b0u, 0xa723fd57u, 0x6118e788u, 0x70506512u, 0x18379a1fu, 0xc1293860u, 0x4b61905fu,
  0xf8a0652eu, 0xeb8560d7u, 0xad0f376eu, 0xac65a5c0u, 0xfecda2feu, 0xa6495323u, 0x44df8932u, 0x3f4341ecu, 0x2516327bu, 0xb46e8713u,
  0x5f095fbcu, 0xdeea128au, 0x22d1fbe3u, 0xff8ad93cu, 0x1e62feaau, 0x3851f4d9u, 0x76128cffu, 0x9ade0582u, 0x5b73f009u, 0x504ad890u,
  0x59975361u, 0x65c5d414u, 0x248acb90u, 0xcd0d5a83u, 0x0e0fd47fu, 0x74ab93adu, 0x1c1da000u, 0x494ff896u, 0x34462f2fu, 0xd308a3e5u,
  0x0fa83babu, 0x980da571u, 0x0e767459u, 0xef70ed41u, 0x99f8dbe3u, 0xea4d9591u, 0x91ef669fu, 0xf4b5cf87u, 0x799a330bu, 0x71031477u,
  0xca84444fu, 0xac0ae8fau, 0xcf21a1d7u, 0x1ba2ade5u, 0xc337f49bu, 0xa80130d6u, 0x08f59b1du, 0x3c4011fau, 0xb170979bu, 0x2c35f6a2u,
  0xb59ddd37u, 0x7b710820u, 0x85ce2257u, 0x5930f350u, 0xde168a72u, 0x9ab00cc6u, 0xd4ae6d4eu, 0x17a1044bu, 0xe90ca54bu, 0x3af68b4au,
  0x4447427au, 0x6f6a7bd2u, 0x157e75bdu, 0x50f97949u, 0xa6b51a1cu, 0xdbf17a6fu, 0x08611ba5u, 0x52bef5eeu, 0x570db829u, 0xb2e3c7c8u,
  0xa0c190acu, 0x145ce291u, 0x74463ec3u, 0xafb45806u, 0x3481e870u, 0x7ca585f8u, 0xc1b67879u, 0x52afe93du, 0xd5872b29u, 0x8df3d951u,
  0xfc0ea201u, 0xfb67bc0du, 0xcf914eb2u, 0x56c1a259u, 0xc73d9b08u, 0x44d7d616u, 0x23f96ed3u, 0x27451078u, 0xf599179bu, 0xeb3681d0u,
  0xf5df8fe4u, 0xb3899e0cu, 0x51762926u, 0x39ca2b58u, 0x68446f16u, 0x80fe0fbau, 0xca721d40u, 0x8adb916eu, 0xa8fb7bdcu, 0xb18bd85eu,
  0x8ce247c0u, 0x188eef6du, 0xe4d075bfu, 0xb147a916u, 0x4726b4acu, 0x12c1fd59u, 0xb3deef99u, 0x2c7fb513u, 0xc8089d4fu, 0x4133c192u,
  0xba475633u, 0xe1428469u, 0x440faac9u, 0x5f73de2du, 0x4c3d8820u, 0xf4606525u, 0x9a2d00a0u, 0x5dc68045u, 0xcea51b42u, 0xd12185b5u,
  0x141c9baeu, 0x05a0f987u, 0x51899e51u, 0x474747c9u, 0x2fb76754u, 0xb20f5630u, 0xc4ee7922u, 0x7498d82fu, 0xe3345fc1u, 0xd6533ea5u,
  0x86cbf0efu, 0xb322884bu, 0xf5ed3d7eu, 0x3efc5893u, 0xf2b9cef8u, 0xa3a471d2u, 0xb5678769u, 0x35264687u, 0xb402f7bcu, 0x67521c62u,
  0x8e506c09u, 0x4aa05ec0u, 0x3ac4145cu, 0x67585c69u, 0x82d0bbcbu, 0x2d5616bfu, 0x128d2590u, 0xc7612d2eu, 0xe1d60f90u, 0xdf6610fdu,
  0x876e93b6u, 0x4bf72502u, 0x8328cc8du, 0xe30185b9u, 0x62bf7230u, 0x29b4fec0u, 0xf812bdd6u, 0x3f5aa49cu, 0xb96c3636u, 0x55daaba9u,
  0x2f111512u, 0x3d051797u, 0xcce65b67u, 0x04c22d39u, 0xa92f609bu, 0xd7a03865u, 0x0eada4b0u, 0xe6a8213eu, 0xa3583a1fu, 0xb6308e03u,
  0x70e68fe0u, 0xe57d4391u, 0x9bb4c9fdu, 0x7c85a520u, 0x5df4987du, 0x3243e749u, 0x564d97fcu, 0xc9fea967u, 0x64d63fbdu, 0x5d774236u,
  0x39af5f9fu, 0x04196b18u, 0xc3c3eb28u, 0xc076c60cu, 0xc693e135u, 0xf8f63932u, 0x609d3cc1u, 0x919f1852u, 0x5d3a2e12u, 0xe45cba4fu,
  0x4c867729u, 0x5c1e8b48u, 0xb8a76ac8u, 0x6ba216fcu, 0xb4f1190eu, 0x086343b8u, 0xa9bfae9fu, 0x4d61f15au, 0x68d9a5b8u, 0xc61f9871u,
  0xd6f22d03u, 0xedc677cdu, 0x92dff114u, 0x4be362efu, 0xe0c4bf54u, 0x5664cde2u, 0x7dffc1f1u, 0x1d65a31du, 0xd3adec7au, 0xec00457cu,
  0x39692111u, 0x2cd35dc0u, 0x6b7fd589u, 0x8abb6263u, 0xb1a8e620u, 0xf5cbb9f1u, 0x4b3a7d5au, 0xe33c0dc9u, 0xf2d5d78au, 0x9129ede8u,
  0x713eedbcu, 0x7fae846cu, 0x827180c5u, 0x4af068b9u, 0x6ecb18f6u, 0xb10c08b8u, 0x1c2c1fc8u, 0xa3f2fabbu, 0x1ab9a33du, 0xfe980daeu,
  0xb405bdc1u, 0x0073f7ffu, 0xfd3e09d6u, 0x9e77149cu, 0xe856f975u, 0x3a3d6ae2u, 0x58011422u, 0x6a450035u, 0x89e6ead7u, 0xbf011331u,
  0x32de7892u, 0xacb7b30du, 0x7b14d983u, 0xbc1349a3u, 0x712892dau, 0x56b130cbu, 0x6ef7619eu, 0x4c2cbdd0u, 0x7d3dbcd2u, 0x641aa80du,
  0x70960afau, 0x9108adceu, 0x7e326a0cu, 0x3273f44eu, 0x70f7a09eu, 0x1ef47c17u, 0x5b81bfd9u, 0x4624d46fu, 0xf541147cu, 0xc9cc5132u,
  0x5ce9e8b4u, 0x1d3345c4u, 0xf51b9cb4u, 0x77b4cc5bu, 0xd605d9ebu, 0xc536ba0cu, 0xe7b32d29u, 0x0ef56e86u, 0x58c21437u, 0x34f03e13u,
  0x0fe9cd5bu, 0x342766a0u, 0x09b46bf4u, 0x926cfc74u, 0x7de84a35u, 0xb84c019fu, 0x58473869u, 0x1f4e6ab7u, 0x52da3b61u, 0xe5a1e52du,
  0xfd03b03fu, 0x2e860ad0u, 0x4bfb4e7eu, 0x9b6d6a31u, 0x01b34063u, 0xbec3c8d8u, 0x63cbdb53u, 0x89728d40u, 0x007852a6u, 0xf30399d9u,
  0x6dca0f89u, 0x2e4efd5eu, 0xd989edcfu, 0xfae6fae0u, 0xf08869b7u, 0x9e79acdbu, 0x1940ee6eu, 0x8f46a8cfu, 0x9681061cu, 0x7f4ae020u,
  0x9faf7623u, 0xdda5445au, 0xb91649e6u, 0x63db9b08u, 0x05979e77u, 0xe2e5826eu, 0x393aee95u, 0x8421dd5bu, 0x52e58203u, 0xd38e31e6u,
  0x7e5bbe93u, 0xb090ad2au, 0xaa31fd2du, 0xe0759b01u, 0x76da34c1u, 0x4e2672ceu, 0xb9b27f33u, 0xa87bb592u, 0x47a68d7fu, 0x15407251u,
  0x3b253968u, 0x7a0f4812u, 0x7c0fcffeu, 0x6fe027e1u, 0x2e0e4f28u, 0x27d1100eu, 0x2f069106u, 0x2b27160eu, 0x84ec41a5u, 0x7dd953d5u,
  0xc9a1aedau, 0xabb32f9bu, 0x1f67bcdeu, 0xb0a9a67du, 0xaa5867eeu, 0x55ce6851u, 0x97a7726du, 0x17e10815u, 0x58007d43u, 0x962fb148u,
  0xb9bb55bdu, 0x37d619c7u, 0xc1d0da0fu, 0x5704a128u, 0xa285094du, 0xf3098fedu, 0x5a64c4b6u, 0xb9c1612eu, 0x97b5d598u, 0xe8b56fb8u,
  0x841efbcdu, 0x2964a0beu, 0xdf2fa415u, 0x46e6df47u, 0x803218c9u, 0x1fef63bdu, 0xaeb1e7d5u, 0x119ce5f8u, 0x7c1f1fa4u, 0xfca193a9u,
  0xe6d47ef3u, 0x06ba467du, 0xa5f1d09eu, 0x89f92d64u, 0xadae4416u, 0x4e76b9b3u, 0xc41a39f9u, 0x3fd81ae8u, 0x1272478fu, 0x4a8144deu,
  0xd799a565u, 0x5e26aaccu, 0xfd65cbffu, 0x2c4bd39eu, 0x9550633du, 0x6e77dde1u, 0x8c1e5db6u, 0x3bceebb8u, 0xec79ada5u, 0xb01f591eu,
  0x21177ba7u, 0x6f6e0793u, 0xfb429955u, 0x566445f9u, 0x81832223u, 0xc0825891u, 0xd97807bcu, 0x0e7fcb7fu, 0x7b70247du, 0x52c8aaa8u,
  0x6c436919u, 0x97f86a15u, 0x978ff3c6u, 0xa979b14du, 0xe39c2373u, 0x5f968ee6u, 0x25f3fbc6u, 0xec2c4ab6u, 0x5485a3f2u, 0xc7fcd7fdu,
  0xac8cb671u, 0x6d1ce02cu, 0x1a01d61fu, 0x3e49ff19u, 0x3160d134u, 0x0459a0f4u, 0x5a65b7c3u, 0xd03cac09u, 0xefeb8240u, 0xc5d6bbf9u,
  0x0c2617b2u, 0x4c9f407fu, 0xa5378c2bu, 0x5c8d570fu, 0xde50874fu, 0x7364d091u, 0xf555a2bfu, 0x74239496u, 0xe6b46005u, 0x0ea573b0u,
  0x3372794eu, 0x23a78016u, 0x8b5d8326u, 0xa27a1f0fu, 0x582620fdu, 0xf23a9f53u, 0x07af7aafu, 0x6588ebdbu, 0x0089e8ddu, 0xbd2bdb00u,
  0xaf443dd1u, 0x7ed2cb5bu, 0x180f698fu, 0x01d301fcu, 0xe4d532ffu, 0x67fa1132u, 0x31bc022au, 0xe32e1a03u, 0xa4c92204u, 0x955aa6c9u,
  0xde9a929eu, 0x8b15d3d9u, 0xcaabfc7au, 0x22a37c38u, 0x09f6b51du, 0xc07e7d79u, 0x70d11a63u, 0xdcab788au, 0x2bb0ccf9u, 0xfd99d532u,
  0x4a9c415eu, 0xfece4772u, 0xd2b678adu, 0x40bd2d88u, 0xea83d778u, 0xfe078b92u, 0xec29a092u, 0xb28be086u, 0x83042583u, 0x79d52549u,
  0xab3ff1c9u, 0x91394c37u, 0x186c9445u, 0x4e8de575u, 0xa9dc9058u, 0xe6a73b51u, 0xc84650d3u, 0x7c7e4f0eu, 0xb2c6b25du, 0x384f80c0u,
  0x41f6a77cu, 0x032f91a7u, 0xa91ca61fu, 0xd74feef8u, 0x0a4eca57u, 0x97f79be1u, 0x64b37b11u, 0xefbbbd8fu, 0xae43ce0cu, 0x21a0180fu,
  0x8e16fd8cu, 0xa7f25e0eu, 0xe8d7c611u, 0x6b65c78du, 0xceb13fecu, 0xea016179u, 0x2a93c2e8u, 0x382ad426u, 0x7917ba77u, 0x18369743u,
  0xfcef7cd6u, 0x1b488b5au, 0xd0daf7eau, 0x1d9a70f7u, 0x241a37cfu, 0x9a3857b7u, 0xa48b27c7u, 0xffe0fccfu, 0xa7667da1u, 0x24d15bbbu,
  0xa0ffb344u, 0xd4cb1845u, 0xf2e131ffu, 0xc481a0e0u, 0xcadc1ec8u, 0x548fa873u, 0x2b110faau, 0xb94e013cu, 0xbd628eddu, 0xa9793e29u,
  0x02ab02f1u, 0x119b5148u, 0xf24f8c8au, 0xe000d132u, 0xaf42e0ffu, 0x9e5158e7u, 0xee4b9be0u, 0x207246ebu, 0x93a489cau, 0x0a16c4f9u,
  0xcbaebb28u, 0xfd8ffc7cu, 0x791eb12eu, 0x35d4fb90u, 0x44763a17u, 0xa7663336u, 0x81875d44u, 0xafe7cd73u, 0x12c1d58au, 0xd9e0c7e3u,
  0x6a1e20b6u, 0xb69dc27cu, 0x43a05e62u, 0x0f43f7fdu, 0xd084070bu, 0xd5cc6841u, 0x896bb994u, 0x87d3d349u, 0x53b8d1feu, 0x0e1bb3abu,
  0xb4b1c6b2u, 0x547fdcaeu, 0x234bc96fu, 0xaeae7e26u, 0x1e24216au, 0xff935655u, 0x7571f500u, 0x4595039du, 0xc14e840bu, 0xa66e2822u,
  0xb5e198cfu, 0x81663890u, 0x3c64bd21u, 0x6746a1c7u, 0xf3bf0b49u, 0xb6dd32eeu, 0xe0a28221u, 0x8f1da5ccu, 0xc6be1e69u, 0x2daf5388u,
  0x7e40d908u, 0xda8b399au, 0x6f9deecdu, 0xfba34182u, 0xea2a8f23u, 0x259c83f5u, 0x7421e1b2u, 0xe2b13bdau, 0x237d96c2u, 0x075b9b6cu,
  0x61637d7du, 0xed0cd3cau, 0x3efb12afu, 0xa0f48db3u, 0xe30c5b78u, 0x3bca1f89u, 0xdf20b679u, 0xce2af6e6u, 0xa7da49e9u, 0x5ea42706u,
  0x8db1855au, 0xf5e85d29u, 0x1e141316u, 0x874b5795u, 0x4711470fu, 0x4d80299fu, 0xebd5894bu, 0xe47bd834u, 0x827da3a5u, 0x17246f4bu,
  0x19dd67e3u, 0x98423894u, 0xcb683c42u, 0xcb258c33u, 0x35f67e4fu, 0x925a2a3eu, 0x17738312u, 0x57f28c42u, 0x972f6d87u, 0x464acddau,
  0x528ce970u, 0x08de3477u, 0x01e422a8u, 0x43346376u, 0x1019abb5u, 0x1df04942u, 0xf8beb301u, 0x2926e2aau, 0x11152cabu, 0x617695c0u,
  0x79038bedu, 0x7218ac46u, 0x91329634u, 0xb8bddac9u, 0xd11d1cc9u, 0x6b3d8e95u, 0x2a9fcaf4u, 0x22197a72u, 0x4b495926u, 0x2e60e8cfu,
  0x2d57d32du, 0x3adc3097u, 0x002786e6u, 0x06d4783fu, 0xd722076du, 0xe8c0a4a4u, 0xb86e7df0u, 0x6382482bu, 0x63cb62bbu, 0x4898a478u,
  0xc7b8760fu, 0x02be0a7du, 0x726a1837u, 0xdebad9dcu, 0x717843a1u, 0xa620153cu, 0x62675570u, 0x143006c3u, 0x0393b759u, 0xdb3bbc25u,
  0x54de022fu, 0x976dfddcu, 0x61b9acf0u, 0x96d8e40cu, 0xf163b64bu, 0x102e9901u, 0x5a537356u, 0xf724bbd1u, 0xa65f0c74u, 0x19bb24c8u,
  0xd546415cu, 0xdc0b4b90u, 0x55eacb2cu, 0x2b661ecau, 0x250c8c0fu, 0xc6342f2fu, 0x5910a157u, 0x022347c6u, 0x5094c6c9u, 0x59e6be6fu,
  0x742b53bau, 0x7d624351u, 0x00a5e64au, 0xcabff7f2u, 0xed3a7a30u, 0xf340aef1u, 0x25b8fc61u, 0x217ad541u, 0x773fbde1u, 0xd2e00b34u,
  0x8c5f5133u, 0xad3ee732u, 0x31d214f9u, 0x0fec3e07u, 0x231cd835u, 0x7c9c1f6au, 0x7226c158u, 0xdfdd3296u, 0xff0ebe79u, 0x603b4bddu,
  0x8c5132b8u, 0xb163b433u, 0x0aac80a5u, 0x993bac88u, 0xa776b524u, 0xb60fb66fu, 0xc57831b7u, 0x034ac8b9u, 0x201c7c0au, 0x88b33a8du,
  0x55098bc9u, 0x9bff7d59u, 0xb5313296u, 0xb05d5dfbu, 0x617b07b4u, 0x6e73f195u, 0xde5af188u, 0x38847f25u, 0x64cc2d4fu, 0x5d7bd984u,
  0xe9c063cdu, 0xb951352cu, 0x8d989595u, 0xf1493518u, 0x6c189a7eu, 0x3d8da9e9u, 0xf7ea75abu, 0xf3452e56u, 0x5c7c59e4u, 0x725afeffu,
  0x865fe5cau, 0xa50a875au, 0x66993498u, 0x885acc7au, 0x30c2981fu, 0x8ee45300u, 0x217a2a29u, 0x4b85ff51u, 0x1958d273u, 0xbdf8d849u,
  0xaf77f46fu, 0x501875edu, 0xa8ba1fccu, 0x9eacb45fu, 0xb73f7d88u, 0xc0617739u, 0x1e9d1357u, 0x9871c12eu, 0x05503d64u, 0x0bacf77fu,
  0x3c58be81u, 0xf88912f9u, 0x1439aad2u, 0xe59c37a9u, 0xac83dd4bu, 0xa2272061u, 0x413e80b8u, 0x31466b0fu, 0xd17de7dfu, 0x4b004adau,
  0xed74e94au, 0x011a66e6u, 0x8269665au, 0xc96dd39du, 0xd315168du, 0xe8ab68a8u, 0x3ab40d1fu, 0xe5d98ceau, 0x3602bfcau, 0x80cf081cu,
  0x089874d0u, 0x0adbbbc8u, 0x3e272e18u, 0x521b71f0u, 0xa35ecbfau, 0x4826d6d3u, 0xf83af888u, 0xbb12dc1fu, 0x58cfcb45u, 0x2de72b31u,
  0x82810fb9u, 0x0918fe55u, 0xb366db06u, 0xe381a5b4u, 0xae463499u, 0xc55d4ea0u, 0xc4b18c14u, 0x09f3fb4au, 0xa96e81c5u, 0x4bccf950u,
  0x4316046bu, 0x27bf90d4u, 0x05ce06a2u, 0x8d12b9c0u, 0x45c56055u, 0x2a678a83u, 0xa090587cu, 0xe7328bbbu, 0xed6fa70eu, 0xb4d5ac91u,
  0x94941773u, 0xdd6b2de9u, 0x072e3a83u, 0xfe43f79eu, 0x67b54de0u, 0xebc99df6u, 0x4a509c75u, 0xf8b62a12u, 0x27d47658u, 0xccc9c877u,
  0x5aaa4821u, 0x68e7d17au, 0xae9d6b64u, 0x56cc6d1fu, 0x64a01eb7u, 0xae5e2e7au, 0xcc835941u, 0x6167f4e2u, 0x9c8cafb7u, 0x4e6fd536u,
  0x7dda9555u, 0x5b6cf5d1u, 0x24ee898cu, 0xb5e684dcu, 0xd20f5805u, 0x8733446eu, 0x0f29d877u, 0x9758f733u, 0x09b070c2u, 0x2d87e103u,
  0x94c5a9bfu, 0x2590bb48u, 0x4a6c02e5u, 0xdf015300u, 0xbce68c2du, 0xf7d7f69fu, 0x5ef7ea3bu, 0x02aab642u, 0xbecf0a52u, 0x28bcc1b8u,
  0x3f4c432cu, 0x5a6f2cbfu, 0x1f23c308u, 0xd61d97c8u, 0x30f3bcdcu, 0x2f93f455u, 0xb5b365beu, 0x7475f7c5u, 0x94bf2fadu, 0xaf46bcc8u,
  0xd5503360u, 0xb673bc19u, 0x8bff3288u, 0xe3894e3fu, 0x0e780172u, 0x01bcd8fbu, 0x7853ec24u, 0xc3dae7ccu, 0xe1b371fau, 0x7e73a8e5u,
  0x452d1b38u, 0xfd28bd06u, 0xafd16d0fu, 0xb20294deu, 0x94afe27eu, 0x2f4c0a0du, 0xdfbfcd66u, 0xd9d7a6c7u, 0xe3327dc5u, 0xca7509a9u,
  0xc93004edu, 0x80c5f2a6u, 0x2953946au, 0x61195e18u, 0xc25c710au, 0x8c1ca072u, 0x71761116u, 0xb81d4059u, 0x7a9c4b92u, 0x1acc9145u,
  0x7f10eba9u, 0x83335c30u, 0xc414c0c4u, 0x9c9e6a28u, 0x33bde51au, 0x72fc9a43u, 0xdf2a4d06u, 0xe0ccb340u, 0xf797d754u, 0x0a47c376u,
};
} // namespace

TEST_CLASS(Pcg32Stream)
{
public:
  TEST_METHOD(TheFirstSixMatchThePublishedVector)
  {
    // PCG's `pcg32-demo`, seeded the same way, prints exactly these. If this fails, whatever is
    // in this tree is not PCG32 however the file is named.
    Neuron::Pcg32 generator{PINNED_SEED, PINNED_STREAM};
    Assert::AreEqual(0xa15c02b7u, generator.Next());
    Assert::AreEqual(0x7b47f409u, generator.Next());
    Assert::AreEqual(0xba1d3330u, generator.Next());
    Assert::AreEqual(0x83d2f293u, generator.Next());
    Assert::AreEqual(0xbfa4784bu, generator.Next());
    Assert::AreEqual(0xcbed606eu, generator.Next());
  }

  TEST_METHOD(TheFirstThousandOutputsArePinned)
  {
    Neuron::Pcg32 generator{PINNED_SEED, PINNED_STREAM};
    for (std::size_t index = 0; index < PINNED_OUTPUT.size(); ++index)
    {
      Assert::AreEqual(PINNED_OUTPUT[index], generator.Next());
    }
  }

  TEST_METHOD(TwoGeneratorsFromOneSeedProduceOneStream)
  {
    // R16's point at the smallest scale it can be asserted: the same seed reproduces.
    Neuron::Pcg32 first{12345, 1};
    Neuron::Pcg32 second{12345, 1};
    for (int draw = 0; draw < 500; ++draw)
    {
      Assert::AreEqual(first.Next(), second.Next());
    }
  }

  TEST_METHOD(TwoStreamsFromOneSeedDoNotCollide)
  {
    // The mechanism R23's world generator will need so that it does not advance the stream the
    // simulation is drawing from. Same seed, different stream, sequences that do not track.
    Neuron::Pcg32 simulation{9001, 0};
    Neuron::Pcg32 world{9001, 1};
    int agreements = 0;
    for (int draw = 0; draw < 500; ++draw)
    {
      if (simulation.Next() == world.Next())
      {
        ++agreements;
      }
    }
    Assert::IsTrue(agreements <= 1, L"two streams from one seed are tracking each other");
  }

  TEST_METHOD(ADifferentSeedIsADifferentStream)
  {
    Neuron::Pcg32 first{1, 0};
    Neuron::Pcg32 second{2, 0};
    Assert::AreNotEqual(first.Next(), second.Next());
  }
};

TEST_CLASS(Pcg32BoundedDraw)
{
public:
  TEST_METHOD(EveryDrawIsInsideItsBound)
  {
    Neuron::Pcg32 generator{777, 3};
    for (std::uint32_t bound = 1; bound <= 64; ++bound)
    {
      for (int draw = 0; draw < 200; ++draw)
      {
        Assert::IsTrue(generator.NextBelow(bound) < bound, L"a bounded draw left its bound");
      }
    }
  }

  TEST_METHOD(TheDegenerateBoundsAreNotDivisionsByZero)
  {
    Neuron::Pcg32 generator{5, 5};
    Assert::AreEqual(0u, generator.NextBelow(0));
    Assert::AreEqual(0u, generator.NextBelow(1));
  }

  TEST_METHOD(ThereIsNoModuloBias)
  {
    // ASSERTED RATHER THAN ASSUMED, which is what M0.7 asks for, and the bound is chosen so the
    // assertion can actually fail. At 3 * 2^30 the quantity 2^32 mod bound is 2^30, so a bare
    // `Next() % bound` returns the lowest third of the range twice as often as either of the
    // others -- about 50, 25, 25. Correct rejection gives three even thirds, and the distance
    // between those two outcomes is enormous next to the sampling noise.
    constexpr std::uint32_t BOUND = 0xC0000000u;
    constexpr std::uint32_t THIRD = BOUND / 3u;
    constexpr int DRAWS = 60000;

    Neuron::Pcg32 generator{2026, 7};
    std::array<int, 3> counts{};
    for (int draw = 0; draw < DRAWS; ++draw)
    {
      const std::uint32_t bucket = generator.NextBelow(BOUND) / THIRD;
      ++counts[static_cast<std::size_t>((bucket > 2u) ? 2u : bucket)];
    }

    // Five percent of a third. Measured worst deviation is well under one percent; a biased
    // implementation misses by fifty.
    constexpr int EXPECTED = DRAWS / 3;
    constexpr int TOLERANCE = EXPECTED / 20;
    for (const int count : counts)
    {
      Assert::IsTrue((count > (EXPECTED - TOLERANCE)) && (count < (EXPECTED + TOLERANCE)), L"the bounded draw is biased");
    }
  }

  TEST_METHOD(ASmallBoundIsEvenlySpread)
  {
    // The everyday case: six buckets over a bound that does not divide 2^32.
    Neuron::Pcg32 generator{31337, 11};
    std::array<int, 6> counts{};
    constexpr int DRAWS = 60000;
    for (int draw = 0; draw < DRAWS; ++draw)
    {
      ++counts[static_cast<std::size_t>(generator.NextBelow(6))];
    }
    constexpr int EXPECTED = DRAWS / 6;
    for (const int count : counts)
    {
      Assert::IsTrue((count > (EXPECTED - (EXPECTED / 10))) && (count < (EXPECTED + (EXPECTED / 10))), L"a small bound is uneven");
    }
  }
};

TEST_CLASS(Pcg32Range)
{
public:
  TEST_METHOD(ItIsInclusiveAtBothEnds)
  {
    Neuron::Pcg32 generator{4242, 2};
    bool sawLowest = false;
    bool sawHighest = false;
    for (int draw = 0; draw < 2000; ++draw)
    {
      const std::int32_t value = generator.NextInRange(-2, 2);
      Assert::IsTrue((value >= -2) && (value <= 2), L"a ranged draw left its range");
      sawLowest = sawLowest || (value == -2);
      sawHighest = sawHighest || (value == 2);
    }
    Assert::IsTrue(sawLowest && sawHighest, L"the range is not inclusive at both ends");
  }

  TEST_METHOD(ASingleValueRangeIsThatValue)
  {
    Neuron::Pcg32 generator{8, 8};
    for (int draw = 0; draw < 50; ++draw)
    {
      Assert::AreEqual(7, generator.NextInRange(7, 7));
      Assert::AreEqual(-7, generator.NextInRange(-7, -7));
    }
  }

  TEST_METHOD(TheFullSignedRangeDoesNotWrapToNothing)
  {
    // The span of INT32_MIN..INT32_MAX is 2^32, which is zero in a uint32. NextInRange has to
    // read that zero as everything rather than as nothing, and this is the case that says so.
    Neuron::Pcg32 generator{99, 4};
    bool sawNegative = false;
    bool sawPositive = false;
    for (int draw = 0; draw < 200; ++draw)
    {
      const std::int32_t value = generator.NextInRange(std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max());
      sawNegative = sawNegative || (value < 0);
      sawPositive = sawPositive || (value > 0);
    }
    Assert::IsTrue(sawNegative && sawPositive, L"the full signed range produced only one sign");
  }

  TEST_METHOD(ANegativeRangeIsUniformToo)
  {
    Neuron::Pcg32 generator{555, 6};
    std::array<int, 4> counts{};
    constexpr int DRAWS = 40000;
    for (int draw = 0; draw < DRAWS; ++draw)
    {
      const std::int32_t value = generator.NextInRange(-4, -1);
      Assert::IsTrue((value >= -4) && (value <= -1));
      ++counts[static_cast<std::size_t>(value + 4)];
    }
    constexpr int EXPECTED = DRAWS / 4;
    for (const int count : counts)
    {
      Assert::IsTrue((count > (EXPECTED - (EXPECTED / 10))) && (count < (EXPECTED + (EXPECTED / 10))));
    }
  }
};

} // namespace NeuronCoreTests
