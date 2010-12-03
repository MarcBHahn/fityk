// This file is part of fityk program. Copyright (C) Marcin Wojdyr
// Licence: GNU General Public License ver. 2+
// $Id$

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <ctype.h>
#include "common.h"
#include "model.h"
#include "func.h"
#include "var.h"
#include "mgr.h"
//#include "guess.h" //estimate_peak_parameters() in guess_f()
#include "logic.h"

using namespace std;

Model::Model(Ftk *F_)
    : F(F_), mgr(*F_)
{
    mgr.register_model(this);
}

Model::~Model()
{
    mgr.unregister_model(this);
}


/// checks if this model depends on the variable with index idx
bool Model::is_dependent_on_var(int idx) const
{
    vector<Variable*> const& vv = mgr.variables();
    vector_foreach (int, i, ff_.idx)
        if (mgr.get_function(*i)->is_dependent_on(idx, vv))
            return true;
    vector_foreach (int, i, zz_.idx)
        if (mgr.get_function(*i)->is_dependent_on(idx, vv))
            return true;
    return false;
}

fp Model::value(fp x) const
{
    x += zero_shift(x);
    fp y = 0;
    vector_foreach (int, i, ff_.idx)
        y += mgr.get_function(*i)->calculate_value(x);
    return y;
}

fp Model::zero_shift(fp x) const
{
    fp z = 0;
    vector_foreach (int, i, zz_.idx)
        z += mgr.get_function(*i)->calculate_value(x);
    return z;
}

void Model::compute_model(vector<fp> &x, vector<fp> &y, int ignore_func) const
{
    // add x-correction to x
    vector_foreach (int, i, zz_.idx)
        mgr.get_function(*i)->calculate_value(x, x);
    // add y-value to y
    vector_foreach (int, i, ff_.idx)
        if (*i != ignore_func)
            mgr.get_function(*i)->calculate_value(x, y);
}

// returns y values in y, x is changed in place to x+Z,
// derivatives are returned in dy_da as a matrix:
// [ dy/da_1 (x_1)  dy/da_2 (x_1)  ...  dy/da_na (x_1)  dy/dx (x_1) ]
// [ dy/da_1 (x_2)  dy/da_2 (x_2)  ...  dy/da_na (x_2)  dy/dx (x_2) ]
// [ ...                                                            ]
// [ dy/da_1 (x_n)  dy/da_2 (x_n)  ...  dy/da_na (x_n)  dy/dx (x_n) ]
void Model::compute_model_with_derivs(vector<fp> &x, vector<fp> &y,
                                      vector<fp> &dy_da) const
{
    assert(y.size() == x.size());
    if (x.empty())
        return;
    fill (dy_da.begin(), dy_da.end(), 0);

    // add x-correction to x
    vector_foreach (int, i, zz_.idx)
        mgr.get_function(*i)->calculate_value(x, x);

    // calculate value and derivatives
    vector_foreach (int, i, ff_.idx)
        mgr.get_function(*i)->calculate_value_deriv(x, y, dy_da, false);
    vector_foreach (int, i, zz_.idx)
        mgr.get_function(*i)->calculate_value_deriv(x, y, dy_da, true);
}

vector<fp>
Model::get_symbolic_derivatives(fp x) const
{
    int n = mgr.parameters().size();
    vector<fp> dy_da(n+1);
    vector<fp> xx(1, x);
    vector<fp> yy(1);
    compute_model_with_derivs(xx, yy, dy_da);
    dy_da.resize(n); //throw out last item (dy/dx)
    return dy_da;
}

vector<fp>
Model::get_numeric_derivatives(fp x, fp numerical_h) const
{
    vector<fp> av_numder = mgr.parameters();
    int n = av_numder.size();
    vector<fp> dy_da(n);
    const fp small_number = 1e-10; //it only prevents h==0
    for (int k = 0; k < n; k++) {
        fp acopy = av_numder[k];
        fp h = max (fabs(acopy), small_number) * numerical_h;
        av_numder[k] -= h;
        mgr.use_external_parameters(av_numder);
        fp y_aless = value(x);
        av_numder[k] = acopy + h;
        mgr.use_external_parameters(av_numder);
        fp y_amore = value(x);
        dy_da[k] = (y_amore - y_aless) / (2 * h);
        av_numder[k] = acopy;
    }
    mgr.use_parameters();
    return dy_da;
}

// estimate max. value in given range (probe at peak centers and between)
fp Model::approx_max(fp x_min, fp x_max) const
{
    mgr.use_parameters();
    fp x = x_min;
    fp y_max = value(x);
    vector<fp> xx;
    vector_foreach (int, i, ff_.idx) {
        fp ctr;
        if (mgr.get_function(*i)->get_center(&ctr)
                && x_min < ctr && ctr < x_max)
            xx.push_back(ctr);
    }
    xx.push_back(x_max);
    sort(xx.begin(), xx.end());
    vector_foreach (fp, i, xx) {
        fp x_between = (x + *i)/2.;
        x = *i;
        fp y = max(value(x_between), value(x));
        if (y > y_max)
            y_max = y;
    }
    return y_max;
}


string Model::get_peak_parameters(vector<fp> const& errors) const
{
    // TODO: use info-numeric-format
    string s;
    s += "# PeakType\tCenter\tHeight\tArea\tFWHM\tparameters...\n";
    vector_foreach (int, i, ff_.idx) {
        Function const* p = mgr.get_function(*i);
        s += p->xname + "  " + p->tp()->name;
        fp a;
        if (p->get_center(&a))
            s += "\t"+S(a);
        else
            s += "\tx";
        if (p->get_height(&a))
            s += "\t"+S(a);
        else
            s += "\tx";
        if (p->get_area(&a))
            s += "\t"+S(a);
        else
            s += "\tx";
        if (p->get_fwhm(&a))
            s += "\t"+S(a);
        else
            s += "\tx";
        s += "\t";
        for (int j = 0; j < p->get_vars_count(); ++j) {
            s += " " + S(p->get_var_value(j));
            if (!errors.empty()) {
                Variable const* var = mgr.get_variable(p->get_var_idx(j));
                if (var->is_simple())
                    s += " +/- " + S(errors[var->get_nr()]);
                else
                    s += " +/- ?";
            }
        }
        s += "\n";
    }
    return s;
}


string Model::get_formula(bool simplify, bool gnuplot_style) const
{
    if (ff_.names.empty())
        return "0";
    string shift;
    vector_foreach (int, i, zz_.idx)
        shift += "+(" + mgr.get_function(*i)->get_current_formula() + ")";
    string x = "(x" + shift + ")";
    string formula;
    vector_foreach (int, i, ff_.idx)
        formula += (i==ff_.idx.begin() ? "" : "+")
                   + mgr.get_function(*i)->get_current_formula(x);
    if (simplify) {
        // check if formula has not-expanded-functions, like Voigt(2,3,4,5)
        bool has_upper = false;
        for (size_t i = 0; i < formula.size(); ++i)
            if (isupper(formula[i])) {
                has_upper = true;
                break;
            }
        // the simplify_formula() is not working with not-expanded-functions
        if (!has_upper)
            formula = simplify_formula(formula);
    }
    if (gnuplot_style) { //gnuplot format is a bit different
        replace_all(formula, "^", "**");
        replace_words(formula, "ln", "log");
        // avoid integer division (1/2 == 0)
        string::size_type pos = 0;
        while ((pos = formula.find('/', pos)) != string::npos) {
            ++pos;
            if (!isdigit(formula[pos]))
                continue;
            while (pos < formula.length() && isdigit(formula[pos]))
                ++pos;
            if (pos == formula.length())
                formula += ".";
            else if (pos != '.')
                formula.insert(pos, ".");
        }
    }
    return formula;
}

string const& Model::get_func_name(char c, int idx) const
{
    vector<string> const& names = get_fz(c).names;
    if (idx < 0)
        idx += names.size();
    if (!is_index(idx, names))
        throw ExecuteError("wrong [index]: " + S(idx));
    return names[idx];
}

fp Model::numarea(fp x1, fp x2, int nsteps) const
{
    x1 += zero_shift(x1);
    x2 += zero_shift(x2);
    fp a = 0;
    vector_foreach (int, i, ff_.idx)
        a += mgr.get_function(*i)->numarea(x1, x2, nsteps);
    return a;
}

