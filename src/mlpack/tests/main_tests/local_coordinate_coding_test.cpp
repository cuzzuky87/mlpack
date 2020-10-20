/**
 * @file tests/local_coordinate_coding_test.cpp
 * @author Nishant Mehta
 *
 * Test for Local Coordinate Coding.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */

// Is the comment below still relevant ?
// Note: We don't use BOOST_REQUIRE_CLOSE in the code below because we need
// to use FPC_WEAK, and it's not at all intuitive how to do that.
#include <mlpack/methods/local_coordinate_coding/lcc.hpp>

#include "../catch.hpp"
#include "../test_catch_tools.hpp"
#include "../serialization_catch.hpp"

using namespace arma;
using namespace mlpack;
using namespace mlpack::regression;
using namespace mlpack::lcc;

void VerifyCorrectnessMainTest(const vec& beta, const vec& errCorr, double lambda)
{
  const double tol = 0.1;
  size_t nDims = beta.n_elem;
  for (size_t j = 0; j < nDims; ++j)
  {
    if (beta(j) == 0)
    {
      // make sure that errCorr(j) <= lambda
      REQUIRE(std::max(fabs(errCorr(j)) - lambda, 0.0) == Approx(0.0).margin(tol));
    }
    else if (beta(j) < 0)
    {
      // make sure that errCorr(j) == lambda
      REQUIRE(errCorr(j) - lambda == Approx(0.0).margin(tol));
    }
    else
    { // beta(j) > 0
      // make sure that errCorr(j) == -lambda
      REQUIRE(errCorr(j) + lambda == Approx(0.0).margin(tol));
    }
  }
}


TEST_CASE("MainLocalCoordinateCodingTestCodingStep", "[LocalCoordinateCodingTest]")
{
  double lambda1 = 0.1;
  uword nAtoms = 10;

  mat X;
  X.load("mnist_first250_training_4s_and_9s.arm");
  uword nPoints = X.n_cols;

  // normalize each point since these are images
  for (uword i = 0; i < nPoints; ++i)
  {
    X.col(i) /= norm(X.col(i), 2);
  }

  mat Z;
  LocalCoordinateCoding lcc(X, nAtoms, lambda1, 10);
  lcc.Encode(X, Z);

  mat D = lcc.Dictionary();

  for (uword i = 0; i < nPoints; ++i)
  {
    vec sqDists = vec(nAtoms);
    for (uword j = 0; j < nAtoms; ++j)
    {
      sqDists[j] = arma::norm(D.col(j) - X.col(i));
    }
    mat Dprime = D * diagmat(1.0 / sqDists);
    mat zPrime = Z.unsafe_col(i) % sqDists;

    vec errCorr = trans(Dprime) * (Dprime * zPrime - X.unsafe_col(i));
    VerifyCorrectnessMainTest(zPrime, errCorr, 0.5 * lambda1);
  }
}

TEST_CASE("MainLocalCoordinateCodingTestDictionaryStep", "[LocalCoordinateCodingTest]")
{
  const double tol = 0.1;

  double lambda = 0.1;
  uword nAtoms = 10;

  mat X;
  X.load("mnist_first250_training_4s_and_9s.arm");
  uword nPoints = X.n_cols;

  // normalize each point since these are images
  for (uword i = 0; i < nPoints; ++i)
  {
    X.col(i) /= norm(X.col(i), 2);
  }

  mat Z;
  LocalCoordinateCoding lcc(X, nAtoms, lambda, 10);
  lcc.Encode(X, Z);
  uvec adjacencies = find(Z);
  lcc.OptimizeDictionary(X, Z, adjacencies);

  mat D = lcc.Dictionary();

  mat grad = zeros(D.n_rows, D.n_cols);
  for (uword i = 0; i < nPoints; ++i)
  {
    grad += (D - repmat(X.unsafe_col(i), 1, nAtoms)) *
        diagmat(abs(Z.unsafe_col(i)));
  }
  grad = lambda * grad + (D * Z - X) * trans(Z);

  REQUIRE(norm(grad, "fro") == Approx(0.0).margin(tol));
}

TEST_CASE("MainLocalCoordinateCodingSerializationTest", "[LocalCoordinateCodingTest]")
{
  mat X = randu<mat>(100, 100);
  size_t nAtoms = 10;

  LocalCoordinateCoding lcc(nAtoms, 0.05, 2 /* don't care about quality */);
  lcc.Train(X);

  mat Y = randu<mat>(100, 200);
  mat codes;
  lcc.Encode(Y, codes);

  LocalCoordinateCoding lccXml(50, 0.1), lccText(12, 0.0), lccBinary(0, 0.0);
  SerializeObjectAll(lcc, lccXml, lccText, lccBinary);

  CheckMatrices(lcc.Dictionary(), lccXml.Dictionary(), lccText.Dictionary(),
      lccBinary.Dictionary());

  mat xmlCodes, textCodes, binaryCodes;
  lccXml.Encode(Y, xmlCodes);
  lccText.Encode(Y, textCodes);
  lccBinary.Encode(Y, binaryCodes);

  CheckMatrices(codes, xmlCodes, textCodes, binaryCodes);

  // Check the parameters, too.
  REQUIRE(lcc.Atoms() == lccXml.Atoms());
  REQUIRE(lcc.Atoms() == lccText.Atoms());
  REQUIRE(lcc.Atoms() == lccBinary.Atoms());

  REQUIRE(lcc.Tolerance() == Approx(lccXml.Tolerance()).epsilon(1e-7));
  REQUIRE(lcc.Tolerance() == Approx(lccText.Tolerance()).epsilon(1e-7));
  REQUIRE(lcc.Tolerance() == Approx(lccBinary.Tolerance()).epsilon(1e-7));

  REQUIRE(lcc.Lambda() == Approx(lccXml.Lambda()).epsilon(1e-7));
  REQUIRE(lcc.Lambda() == Approx(lccText.Lambda()).epsilon(1e-7));
  REQUIRE(lcc.Lambda() == Approx(lccBinary.Lambda()).epsilon(1e-7));

  REQUIRE(lcc.MaxIterations() == lccXml.MaxIterations());
  REQUIRE(lcc.MaxIterations() == lccText.MaxIterations());
  REQUIRE(lcc.MaxIterations() == lccBinary.MaxIterations());
}

/**
 * Test that LocalCoordinateCoding::Train() returns finite final objective
 * value.
 */
TEST_CASE("MainLocalCoordinateCodingTrainReturnObjective", "[LocalCoordinateCodingTest]")
{
  double lambda1 = 0.1;
  uword nAtoms = 10;

  mat X;
  X.load("mnist_first250_training_4s_and_9s.arm");
  uword nPoints = X.n_cols;

  // Normalize each point since these are images.
  for (uword i = 0; i < nPoints; ++i)
  {
    X.col(i) /= norm(X.col(i), 2);
  }

  LocalCoordinateCoding lcc(nAtoms, lambda1, 10);
  double objVal = lcc.Train(X);

  REQUIRE(std::isfinite(objVal) == true);
}
