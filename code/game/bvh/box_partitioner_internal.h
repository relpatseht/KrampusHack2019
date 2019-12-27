#pragma once

#include <array>

typedef unsigned uint;

template<size_t D>
using VecD = std::array<float*, D>;

template<size_t D>
struct Boxes
{
	const uint count;
	VecD<D> mins;
	VecD<D> maxs;
	VecD<D> weightedCenters;
	float *masses;

	Boxes(uint count) : count(count) {};
};

struct BoxMeans
{
	uint *nearestMeans;
	float *meanDists;
};

template<size_t D>
struct Means
{
	const uint k;
	VecD<D> means;

	Means(uint k) : k(k) {}
};

template<size_t D>
struct BoxPartitionDriver
{
	virtual void MakeIndexList(unsigned *start, unsigned count, unsigned valStart) = 0;
	virtual void InitializeBoxes(Boxes<D> *inoutBoxes) = 0;
	virtual void InitializeMeans(const Boxes<D> &boxes, Means<D> *outMeans, BoxMeans *outBoxMeans, uint *indices) = 0;
	virtual bool UpdateNearestMeans(const Boxes<D> &boxes, const Means<D> &means, BoxMeans *inoutBoxMeans) = 0;
	virtual void ComputeMeans(const Boxes<D> &boxes, const uint *partitions, Means<D> *outMeans) = 0;
	virtual uint AlignBoxCount(uint boxCount) = 0;
};
