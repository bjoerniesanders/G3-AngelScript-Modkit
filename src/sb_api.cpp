#include "sb_api.h"

namespace og3p {

SbApi g_sb;

template <class F> static void Get(HMODULE m, F& f, const char* name, std::string& missing) {
    f = reinterpret_cast<F>(GetProcAddress(m, name));
    if (!f) { missing += name; missing += ' '; }
}

bool SbApi::Resolve(HMODULE m) {
    module = m;
    missing.clear();
    Get(m, strCtor, "??0bCString@@QAE@XZ", missing);
    Get(m, strCtorText, "??0bCString@@QAE@PBD@Z", missing);
    Get(m, strCopy, "??0bCString@@QAE@ABV0@@Z", missing);
    Get(m, strDtor, "??1bCString@@QAE@XZ", missing);
    Get(m, strAssign, "??4bCString@@QAEAAV0@ABV0@@Z", missing);
    Get(m, strAddAssign, "??YbCString@@QAEAAV0@ABV0@@Z", missing);
    Get(m, strGetText, "?GetText@bCString@@QBEPBDXZ", missing);
    Get(m, strGetLength, "?GetLength@bCString@@QBEHXZ", missing);
    Get(m, strIsEmpty, "?IsEmpty@bCString@@QBE_NXZ", missing);
    Get(m, strCompare, "?Compare@bCString@@QBEHABV1@@Z", missing);
    Get(m, strCompareNoCase, "?CompareNoCase@bCString@@QBEHABV1@@Z", missing);
    Get(m, strFind, "?Find@bCString@@QBEHPBDH_N@Z", missing);
    Get(m, strLeft, "?Left@bCString@@QBE?AV1@H@Z", missing);
    Get(m, strRight, "?Right@bCString@@QBE?AV1@H@Z", missing);
    Get(m, strMid, "?Mid@bCString@@QBE?AV1@HH@Z", missing);
    Get(m, strToUpper, "?ToUpper@bCString@@QAEXXZ", missing);
    Get(m, strToLower, "?ToLower@bCString@@QAEXXZ", missing);
    Get(m, vecMagnitude, "?GetMagnitude@bCVector@@QBEMXZ", missing);
    Get(m, vecSquareMagnitude, "?GetSquareMagnitude@bCVector@@QBEMXZ", missing);
    Get(m, vecNormalize, "?Normalize@bCVector@@QAEABV1@XZ", missing);
    Get(m, vecGetNormalized, "?GetNormalized@bCVector@@QBE?AV1@XZ", missing);
    Get(m, vecDot, "?GetDotProduct@bCVector@@QBEMABV1@@Z", missing);
    Get(m, vecCross, "?GetCrossProduct@bCVector@@QBE?AV1@ABV1@@Z", missing);
    Get(m, vec2Magnitude, "?GetMagnitude@bCVector2@@QBEMXZ", missing);
    Get(m, vec2Normalize, "?Normalize@bCVector2@@QAEABV1@XZ", missing);
    Get(m, vec4Magnitude, "?GetMagnitude@bCVector4@@QBEMXZ", missing);
    Get(m, vec4Normalize, "?Normalize@bCVector4@@QAEABV1@XZ", missing);
    Get(m, quatClear, "?Clear@bCQuaternion@@QAEXXZ", missing);
    Get(m, quatNormalize, "?Normalize@bCQuaternion@@QAEXXZ", missing);
    Get(m, quatInvert, "?Invert@bCQuaternion@@QAEXXZ", missing);
    Get(m, quatMagnitude, "?GetMagnitude@bCQuaternion@@QBEMXZ", missing);
    Get(m, quatMul, "??DbCQuaternion@@QBE?AV0@ABV0@@Z", missing);
    Get(m, quatFromMatrix3, "?SetQuaternion@bCQuaternion@@QAEXABVbCMatrix3@@@Z", missing);
    Get(m, m3Identity, "?SetIdentity@bCMatrix3@@QAEXXZ", missing);
    Get(m, m3Transpose, "?Transpose@bCMatrix3@@QAEXXZ", missing);
    Get(m, m3Invert, "?Invert@bCMatrix3@@QAEXXZ", missing);
    Get(m, m3MulM3, "??DbCMatrix3@@QBE?AV0@ABV0@@Z", missing);
    Get(m, m3MulVec, "??DbCMatrix3@@QBE?AVbCVector@@ABV1@@Z", missing);
    Get(m, m4Identity, "?SetIdentity@bCMatrix@@QAEXXZ", missing);
    Get(m, m4Transpose, "?Transpose@bCMatrix@@QAEXXZ", missing);
    Get(m, m4Invert, "?Invert@bCMatrix@@QAEXXZ", missing);
    Get(m, m4Det, "?GetDeterminant@bCMatrix@@QBEMXZ", missing);
    Get(m, m4MulM4, "??DbCMatrix@@QBE?AV0@ABV0@@Z", missing);
    Get(m, m4MulVec, "??DbCMatrix@@QBE?AVbCVector@@ABV1@@Z", missing);
    Get(m, m4Translation, "?SetToTranslation@bCMatrix@@QAEXABVbCVector@@@Z", missing);
    Get(m, m4Scaling, "?SetToScaling@bCMatrix@@QAEXABVbCVector@@@Z", missing);
    Get(m, m4FromQuat, "?SetToRotation@bCMatrix@@QAEXABVbCQuaternion@@@Z", missing);
    Get(m, m4RotX, "?SetToRotationAxisX@bCMatrix@@QAEXM@Z", missing);
    Get(m, m4RotY, "?SetToRotationAxisY@bCMatrix@@QAEXM@Z", missing);
    Get(m, m4RotZ, "?SetToRotationAxisZ@bCMatrix@@QAEXM@Z", missing);
    Get(m, m3RotX, "?SetToRotationAxisX@bCMatrix3@@QAEXM@Z", missing);
    Get(m, m3RotY, "?SetToRotationAxisY@bCMatrix3@@QAEXM@Z", missing);
    Get(m, m3RotZ, "?SetToRotationAxisZ@bCMatrix3@@QAEXM@Z", missing);
    Get(m, m3FromQuat, "?SetToRotation@bCMatrix3@@QAEXABVbCQuaternion@@@Z", missing);
    Get(m, geInfo, "GE_MESSAGEF_INFO", missing);
    return strCtor && strCtorText && strCopy && strDtor && strAssign && strGetText && strGetLength;
}

}  // namespace og3p
