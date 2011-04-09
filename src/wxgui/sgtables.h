/*  sgtables.h
 *  Author: Marcin Wojdyr (only the code, not data)
 *  Licence: GNU General Public License ver. 2+
 * 
 * Data generated by cctbx (http://cctbx.sourceforge.net):
 * - tables that contain symbols of 230 crystallographic space groups
 *   and symbols of 530 space group settings, all generated by cctbx.
 * - seitz matrices for each of 530 settings,
 * - translation of setting numbers used in PowderCell's .cel files
 *   to more popular setting symbols
 */

#ifndef FITYK_SGTABLES_H_
#define FITYK_SGTABLES_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct
{
    int R[9];
    int T[3]; /* divide by 12. before use */
}
SeitzMatrix;

typedef struct
{
    char txt[8];
    char r1, r2, r3, t;
}
ThirdOfSeitzMx;

typedef struct
{
    int sgnumber; /* space group number (1-230) */
    char ext; /* '1', '2', 'H', 'R' or '\0' */
    char qualif[5]; /* e.g. "-cba" or "b1" */
    char HM[11]; /* H-M symbol; nul-terminated string */
    char Hall[16]; /* Hall symbol; nul-terminated string */
    /* encoded seitz matrices, without the unit matrix */
    const char* encoded_seitz;
}
SpaceGroupSetting;

extern const ThirdOfSeitzMx seitz_mx_codes[];
extern const SpaceGroupSetting space_group_settings[];
extern const char* SchoenfliesSymbols[];
extern const char* SchoenfliesSymbolsAsUTF8[];

int get_seitz_mx_count(const SpaceGroupSetting *sgs);
void decode_seitz_mx(char x, char y, char z, SeitzMatrix *sm);
void get_seitz_mx(const SpaceGroupSetting *sgs, int n, SeitzMatrix *sm);
const SpaceGroupSetting* find_first_sg_with_number(int sgn);
const SpaceGroupSetting* get_sg_from_powdercell_rgnr(int sgn, int setting);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* FITYK_SGTABLES_H_ */
