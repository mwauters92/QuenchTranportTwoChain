#ifndef __BDGBASIS_H_CMC__
#define __BDGBASIS_H_CMC__
#include "itensor/all.h"
#include "ReadWriteFile.h"
using namespace itensor;
using namespace std;

//           [ -mu      -t       0    |  0     -Delta   0     ]
//           [ -t       -mu     -t    |  Delta   0     -Delta ]
//           [  0       -t            |  0      Delta         ]
// H = 0.5 * [ ---------------------------------------------- ]
//           [  0       Delta    0    |  mu      t      0     ]
//           [  -Delta    0    Delta  |  t       mu     t     ]
//           [  0      -Delta         |  0       t            ]
//
Matrix BdG_Hamilt (int L, Real t, Real mu, Real Delta)
{
    Matrix H (2*L, 2*L);
    for(int i = 0; i < L; i++)
    {
        // Upper left block
        H(i,i) = -mu;
        // Lower right block
        H(i+L,i+L) = mu;

        if (i != L-1)
        {
            // Upper left block
            H(i,i+1) = -t;
            H(i+1,i) = -t;
            // Lower right block
            int j = i+L;
            H(j,j+1) = t;
            H(j+1,j) = t;
            // Upper right block
            H(i,j+1) = -Delta;
            H(i+1,j) = Delta;
            // Lower left block
            H(j,i+1) = Delta;
            H(j+1,i) = -Delta;
        }
    }
    return 0.5*H;
}

void set_t_BdG_Hamilt (Matrix& H, Real t, int i, int j)
{
    int L = ncols(H) / 2;
    // Upper left block
    H(i,j) = -t;
    H(j,i) = -t;
    // Lower right block
    int i2 = i+L,
        j2 = j+L;
    H(i2,j2) = t;
    H(j2,i2) = t;
}

void set_mu_BdG_Hamilt (Matrix& H, Real mu, int i)
{
    int L = ncols(H) / 2;
    H(i,i) = -mu;
    H(i+L,i+L) = mu;
}

void set_Delta_BdG_Hamilt (Matrix& H, Real Delta, int i, int j)
{
    int L = ncols(H) / 2;
    int i2 = i+L,
        j2 = j+L;
    // Upper right block
    H(i,j2) = -Delta;
    H(j,i2) = Delta;
    // Lower left block
    H(i2,j) = Delta;
    H(j2,i) = -Delta;
}

Matrix BdG_Hamilt2 (int L, Real t, Real mu, Real Delta)
{
    Matrix H (2*L, 2*L);
    for(int i = 0; i < L; i++)
        set_mu_BdG_Hamilt (H, mu, i);
    for(int i = 0; i < L-1; i++)
    {
        set_t_BdG_Hamilt (H, t, i, i+1);
        set_Delta_BdG_Hamilt (H, Delta, i, i+1);
    }
    return 0.5*H;
}

// Use only the positive-energy states, ordering from lowest to highest energy
class BdGBasis
{
    public:
        BdGBasis () {}
        BdGBasis (const string& name, const Matrix& H);
        BdGBasis (const string& name, int L, Real t, Real mu, Real Delta)
        : BdGBasis (name, BdG_Hamilt (L,t,mu,Delta))
        {}

        tuple<vector<Real>,vector<int>,vector<string>> C (int i);

        // Functions that every basis class must have
        const string&                name     ()                const { return _name; }
        vector<tuple<int,auto,bool>> C_op     (int i, bool dag) const;
        vector<tuple<int,auto,bool>> gamma_op (int k, bool dag) const;
        // en(i) is the energy in H_BdG = sum_i^N { en(i) gamma_i^dag gamma_i } - sum_i^N { (1/2) en(i) }
        Real                         en       (int k)           const { return _ens(k-1); }
        Real                         mu       (int k)           const { mycheck (k <= this->size(), "Out of range"); return -2. * _H(k-1,k-1); }
        int                          size     ()                const { return _ens.size(); }
        auto const&                  H        ()                const { return _H; }
        auto const&                  H        (int i, int j)    const { return _H(i,j); }
        auto const&                  u        (int i, int j)    const { return _u(i,j); }
        auto const&                  v        (int i, int j)    const { return _v(i,j); }
        auto const&                  ens      ()                const { return _ens; }
        Matrix                       U        ()                const;

        void write (ostream& s) const
        {
            itensor::write(s,_name);
            itensor::write(s,_u);
            itensor::write(s,_v);
            itensor::write(s,_ens);
            itensor::write(s,_H);
        }
        void read (istream& s)
        {
            itensor::read(s,_name);
            itensor::read(s,_u);
            itensor::read(s,_v);
            itensor::read(s,_ens);
            itensor::read(s,_H);
        }

    protected:
        string _name;
        Vector _ens;
        Matrix _u, _v, _H;
};

Vector particle_hole_transform (const Vector& v)
{
    int N = v.size()/2;
    Vector w (2*N);
    subVector (w,0,N) &= subVector (v,N,2*N);
    subVector (w,N,2*N) &= subVector (v,0,N);
    return w;
}

// For each BdG basis state, suppose |phi_i> has energy E_i,
// S|phi_i> will have energy -E_i, where S is the particle-hole transformation.
// Suppose |phi_i> has dimension 2N.
// S|phi_i> swaps the first N subvector to the last N subvector of |phi_i>.
//
// The particle-hole correspondence for the E_i and -E_i basis states is autoomatically satisfied for the non-zero-energy modes.
// However for zero-energy (Majorana) modes, since |phi_i> and S|phi_i> are degenerate,
// numerically we will get in geenral arbitrary superpositions of them,
// so one needs to symmetrice them explicitly.
Matrix symmetrice_zero_energy_modes (const Vector& ens, Matrix U, Real zero_crit=1e-12)
{
    mycheck (ens.size() == nrows(U), "Size not match");
    int N = ens.size()/2;

    // Target the zero-energy modes
    vector<int> is;
    vector<Vector> states;
    for(int i = 0; i < 2*N; i++)
    {
        if (abs(ens(i)) < zero_crit)
        {
            cout << "MBS energy" << ens(i) << endl;
            is.push_back (i);
            states.emplace_back (column (U, i));
        }
    }
    if (states.size() == 0)
        return U;
    //mycheck (states.size() == 2, "Allow only upto 2 zero-energy modes");
    mycheck (states.size() % 2 == 0, "Must be even number of zero-energy modes");

    // Define particle-hole transformation matrix S in zero-energy modes subspace
    int N0 = states.size();
    Matrix S (N0, N0);
    for(int i = 0; i < N0; i++)
    {
        auto phi = states.at(i);
        for(int j = 0; j < N0; j++)
        {
            auto const& phip = states.at(j);
            auto phit = particle_hole_transform (phi);
            auto si = phip * phit;
            S(i,j) = si;
        }
    }

    // Diagonalize S
    Matrix W;
    Vector eigvals;
    diagHermitian (S, W, eigvals);

    // Get the eigenstates of S
    auto e_pos = eigvals(0);
    auto e_neg = eigvals(1);
    auto w_pos = Vector (column (W, 0));
    auto w_neg = Vector (column (W, 1));
    if (e_pos < 0.)
    {
        swap (e_pos, e_neg);
        swap (w_pos, w_neg);
    }
    Vector v_pos (2*N),
           v_neg (2*N);
    for(int i = 0; i < N0; i++)
    {
        v_pos += w_pos (i) * states.at(i);
        v_neg += w_neg (i) * states.at(i);
    }
    mycheck (abs(e_pos-1) < 1e-14 and abs(e_neg+1) < 1e-14, "particle-hole eigenvalues error");

    // Suppose S has eigenvectors |u> and |v> with eigenvalues +1 and -1
    // The (unnormalized) symmetric zero-energy modes are |u>+|v> and |u>-|v>
    auto phi1 = v_pos + v_neg;
    phi1 /= norm(phi1);
    auto phi2 = particle_hole_transform (phi1);

    // Update the zero-energy states in U
    column (U,is.at(0)) &= phi1;
    column (U,is.at(1)) &= phi2;

    return U;
}

void check_orthogonal_to_particle_hole_transform (const Vector& v, Real crit=1e-8)
{
    Vector w = particle_hole_transform (v);
    auto o = w*v;
    if (!(abs(o) < crit))
    {
        cout << "Not orthogonal to its particle-hole transformed state" << endl;
        cout << v << endl;
        cout << w << endl;
        cout << abs(o) << endl;
        throw;
    }
}

BdGBasis :: BdGBasis (const string& name, const Matrix& H)
: _name (name)
, _H (H)
{
    Matrix U;
    Vector ens;
    diagHermitian (_H, U, ens);
    // ens(q) for q=0,1,...,2N-1 is the energy in descending order (highest to lowest).
    // The energy for q=0,...N-1 is positive
    //                q=N,...2N-1 is negative


    U = symmetrice_zero_energy_modes (ens, U);

    auto tmp = U(0,0);
    if constexpr (!is_same_v <decltype(tmp), Real>)
    {
        cout << "The current version is only for unitary matrix of Real type" << endl;
        cout << "Type found is: " << typeid(tmp).name() << endl;
        throw;
    }

    // _ens are for the positive energies in ascending order (lowest to highest)
    //  U = [ _u  _v* ] is the unitrary matrix to diagonalize H
    //      [ _v  _u* ]
    // Take only the positive-energy states, i.e. the first N columns, to define _u and _v.
    int N = ens.size()/2;
    _ens = Vector (N);
    _u = Matrix (N,N);
    _v = Matrix (N,N);
    int j = 0;
    for(int i = N-1; i >= 0; i--)
    {
        // Check the state is orthogonal to its particle-hole transformed state
        auto phi = Vector (column (U, i));
        //cout << "state being tested " << i << " with energy " <<ens(i) <<  endl;
        if (ens(i) < 1e-6)
        {
            check_orthogonal_to_particle_hole_transform (phi, 1e-5 );
        }
        else
        {
            check_orthogonal_to_particle_hole_transform (phi);
        }
        _ens(j) = 2.*ens(i);
        column (_u,j) &= subVector (phi,0,N);
        column (_v,j) &= subVector (phi,N,2*N);
        j++;
    }

    // Check the unitrary matrices U = [ u  v* ]
    //                                 [ v  u* ]
    Matrix Uc = this->U();
    auto Hd = transpose(Uc) * _H * Uc;
    for(int i = 0; i < N; i++)
    {
        Hd(i,i) -= 0.5*_ens(i);
        Hd(i+N,i+N) += 0.5*_ens(i);
    }
    mycheck (abs(norm(Hd)) < 1e-10, "Construct unitray matrix failed");
}

// i is the site index in real space
// Return k, coef, dagger
//
// [ C    ]  = [ u v* ] [ gamma     ]
// [ Cdag ]    [ v u* ] [ gamma^dag ]
//
// C_i = sum_k^N { u(i,k) gamma + v*(i,k) gamma^dag }
vector<tuple<int,auto,bool>> BdGBasis :: C_op (int i, bool dag) const
{
    int N = _ens.size(); 
    mycheck (i >= 1 and i <= N, "out of range");     // i is from 1 to N

    auto tmp = _u(0,0);
    vector<tuple<int,decltype(tmp),bool>> k_coef_dag;
    // For C
    for(int k = 0; k < N; k++)
    {
        auto uk = _u(i-1,k);
        auto vk = _v(i-1,k);
        if (abs(uk) > 1e-14)
            k_coef_dag.emplace_back (k+1, uk, false);
        if (abs(vk) > 1e-14)
            k_coef_dag.emplace_back (k+1, iut::conj(vk), true);
    }
    // If Cdag
    if (dag)
    {
        for(auto& [k, coef, dagk] : k_coef_dag)
        {
            coef = iut::conj (coef);
            dagk = !dagk;
        }
    }

    return k_coef_dag;
}

//                                  [ u v* ]
// [ gamma^dag gamma ] = [ C^dag C] [ v u* ]
//
// gamma^dag_k = sum_i^N { u(i,k) C_i^dag + v(i,k) C_i }
vector<tuple<int,auto,bool>> BdGBasis :: gamma_op (int k, bool dag) const
{
    int N = _ens.size();
    mycheck (k >= 1 and k <= N, "out of range");     // i is from 1 to N

    auto tmp = _u(0,0);
    vector<tuple<int,decltype(tmp),bool>> i_coef_dag;
    // For gamma^dag
    for(int i = 0; i < N; i++)
    {
        auto uk = _u(i,k-1);
        auto vk = _v(i,k-1);
        if (abs(uk) > 1e-14)
            i_coef_dag.emplace_back (i+1, uk, true);
        if (abs(vk) > 1e-14)
            i_coef_dag.emplace_back (i+1, vk, false);
    }
    // If gamma
    if (!dag)
    {
        for(auto& [i, coef, dagk] : i_coef_dag)
        {
            coef = iut::conj (coef);
            dagk = !dagk;
        }
    }
    return i_coef_dag;
}

// Unitrary matrix U = [ u  v* ]
//                     [ v  u* ]
Matrix BdGBasis :: U () const
{
    int N = ncols (_u);
    Matrix Uc (2*N, 2*N);
    subMatrix(Uc,0,N,0,N)     &= _u;
    subMatrix(Uc,N,2*N,0,N)   &= _v;
    subMatrix(Uc,0,N,N,2*N)   &= conj (_v);
    subMatrix(Uc,N,2*N,N,2*N) &= conj (_u);
    return Uc;
}

namespace iut
{
auto write (ostream& s, const BdGBasis& t)
{
    t.write (s);
}
auto read (istream& s, BdGBasis& t)
{
    t.read (s);
}
} // namespace

// Original Hamiltonian in the BdG basis.
// Do not confuse with the BdG Hamiltonian.
// H_BdG = \sum_i^N epsilon_i gamma^dag_i gamma_i - \sum_i^N (1/2) epsilon_i
// H = H_BdG - (1/2) * sum_i^N mu_i
template <typename SiteType>
AutoMPO H_AMPO_BdG_basis (const SiteType& sites, const BdGBasis& bdg)
{
    AutoMPO ampo (sites);
    int N = length (sites);
    for(int i = 1; i <= N; i++)
    {
        ampo += bdg.en(i),"N",i;
        ampo += -0.5 * (bdg.en(i) + bdg.mu(i)), "I", i;
    }
    return ampo;
}

// Ground state energy for the orignal Hamiltonian (not the BdG Hamiltonian)
Real ground_state_energy (const BdGBasis& b)
{
    Real en = 0.;
    for(int i = 0; i < b.size(); i++)
    {
        en -= 0.5*b.en(i+1) - b.H(i,i);
    }
    return en;
}

void print_ops (const vector<tuple<int,auto,bool>>& ops)
{
    cout << "site, coef, dag" << endl;
    for(auto& [k, coef, dagk] : ops)
    {
        cout << k << "  " << coef << "  " << dagk << endl;
    }
}
#endif
