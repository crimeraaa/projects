#ifndef PROJECTS_MATH_CHECKED_H
#define PROJECTS_MATH_CHECKED_H

#include <projects.h>


/** @brief `*dst = a + b` with an overflow check for `a + b > max(u64)`.
 *
 * @param [out] dst Always assigned no matter what.
 *
 * @return
 *  `true` if the addition resulted in unsigned overflow else `false`.
 */
bool
u64_checked_add(u64 *dst, u64 a, u64 b);


/** @brief `*dst = a + b + carry` with an overflow check for
 *  `a + b + carry > max(u64)`.
 *
 * @param [out] dst     Always assigned no matter what.
 * @param [in]  carry   Can be any value, but boolean is most optimal.
 *
 * @return
 *  `true` if the addition resulted in unsigned overflow else `false`.
 */
bool
u64_checked_add_carry(u64 *dst, u64 a, u64 b, u64 carry);


/** @brief `*dst = a - b` with an overflow check for `a - b < 0`.
 *
 * @param [out] dst Always assigned no matter what.
 *
 * @return
 *  `true` if the subtraction resulted in unsigned overflow else `false`.
 */
bool
u64_checked_sub(u64 *dst, u64 a, u64 b);


/** @brief `*dst = a - b - carry` with an overflow check for
 *  `a - b - carry < 0`.
 *
 * @param [out] dst     Always assigned no matter what.
 * @param [in]  carry   Can be any value, but boolean is most optimal.
 *
 * @return
 *  `true` if the subtraction resulted in unsigned overflow else `false`.
 */
bool
u64_checked_sub_carry(u64 *dst, u64 a, u64 b, u64 carry);


/** @brief `*dst = a * b` with an overflow check for `a * b > max(u64)`.
 *
 * @param [out] dst Always assigned no matter what.
 *
 * @return
 *  `true` if the multiplication resulted in unsigned overflow else false`.
 */
bool
u64_checked_mul(u64 *dst, u64 a, u64 b);

#endif // PROJECTS_MATH_CHECKED_H
