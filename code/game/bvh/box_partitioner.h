#pragma once

size_t ComputePartitionLinesWorkMemSize(unsigned boxCount);
size_t ComputePartitionBoxes2DWorkMemSize(unsigned boxCount);
size_t ComputePartitionBoxes3DWorkMemSize(unsigned boxCount);

template<unsigned D>
size_t ComputePartitionBoxesWorkMemSize(unsigned boxCount)
{
	typedef size_t(*WorkMemFunc)(unsigned boxCount);
	static const WorkMemFunc WorkMemSize[] = { &ComputePartitionLinesWorkMemSize, &ComputePartitionBoxes2DWorkMemSize, &ComputePartitionBoxes3DWorkMemSize };

	static_assert(D >= 1 && D <= sizeof(WorkMemSize) / sizeof(WorkMemSize[0]) + 1, "No compute work mem implementation for given dimensions");

	return WorkMemSize[D-1]( boxCount);
}

bool PartitionLines(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions, void *workMem);
bool PartitionBoxes2D(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions, void *workMem);
bool PartitionBoxes3D(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions, void *workMem);

bool PartitionLines(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions);
bool PartitionBoxes2D(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions);
bool PartitionBoxes3D(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions);


template<unsigned K>
bool PartitionLines(float * const * boxMins, float * const * boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned(&outPartitions)[K], void *workMem)
{
	return PartitionLines(K, reinterpret_cast<const float * const *>(boxMins), reinterpret_cast<const float * const *>(boxMaxs), boxCount, outIndices, reinterpret_cast<unsigned *>(outPartitions), workMem);
}

template<unsigned K>
bool PartitionBoxes2D(float * const * boxMins, float * const * boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned(&outPartitions)[K], void *workMem)
{
	return PartitionBoxes2D(K, boxMins, boxMaxs, boxCount, outIndices, reinterpret_cast<unsigned *>(outPartitions), workMem);
}

template<unsigned K>
bool PartitionBoxes3D(float * const * boxMins, float * const * boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned(&outPartitions)[K], void *workMem)
{
	return PartitionBoxes3D(K, boxMins, boxMaxs, boxCount, outIndices, reinterpret_cast<unsigned *>(outPartitions), workMem);
}

template<unsigned K>
bool PartitionLines(float * const * boxMins, float * const * boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned(&outPartitions)[K])
{
	return PartitionLines(K, boxMins, boxMaxs, boxCount, outIndices, reinterpret_cast<unsigned *>(outPartitions));
}

template<unsigned K>
bool PartitionBoxes2D(float * const * boxMins, float * const * boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned(&outPartitions)[K])
{
	return PartitionBoxes2D(K, boxMins, boxMaxs, boxCount, outIndices, reinterpret_cast<unsigned *>(outPartitions));
}

template<unsigned K>
bool PartitionBoxes3D(float * const * boxMins, float * const * boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned(&outPartitions)[K])
{
	return PartitionBoxes3D(K, boxMins, boxMaxs, boxCount, outIndices, reinterpret_cast<unsigned *>(outPartitions));
}

template<unsigned D>
bool PartitionBoxes(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions, void *workMem)
{
	typedef bool(*PartFunc)(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions, void *workMem);
	static const PartFunc Partitioners[] = { &PartitionLines, &PartitionBoxes2D, &PartitionBoxes3D };

	static_assert(D >= 1 && D <= sizeof(Partitioners) / sizeof(Partitioners[0]) + 1, "No partition implementation for given dimensions");

	return Partitioners[D-1](k, boxMins, boxMaxs, boxCount, outIndices, outPartitions, workMem);
}

template<unsigned D>
bool PartitionBoxes(unsigned k, float *(&boxMins)[D], float *(&boxMaxs)[D], unsigned boxCount, unsigned *outIndices, unsigned *outPartitions, void *workMem)
{
	return PartitionBoxes<D>(k, &boxMins[0], &boxMaxs[0], boxCount, outIndices, outPartitions, workMem);
}

template<unsigned D>
bool PartitionBoxes(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions)
{
	typedef bool(*PartFunc)(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions);
	static const PartFunc Partitioners[] = { &PartitionLines, &PartitionBoxes2D, &PartitionBoxes3D };

	static_assert(D >= 1 && D <= sizeof(Partitioners) / sizeof(Partitioners[0]) + 1, "No partition implementation for given dimensions");

	return Partitioners[D-1](k, boxMins, boxMaxs, boxCount, outIndices, outPartitions);
}

template<unsigned D>
bool PartitionBoxes(unsigned k, float *(&boxMins)[D], float *(&boxMaxs)[D], unsigned boxCount, unsigned *outIndices, unsigned *outPartitions)
{
	return PartitionBoxes<D>(k, &boxMins[0], &boxMaxs[0], boxCount, outIndices, outPartitions);
}

template<unsigned D, unsigned K>
bool PartitionBoxes(float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions, void *workMem)
{
	typedef bool(*PartFunc)(float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions, void *workMem);
	static const PartFunc Partitioners[] = { &PartitionLines<K>, &PartitionBoxes2D<K>, &PartitionBoxes3D<K> };

	static_assert(D >= 1 && D <= sizeof(Partitioners) / sizeof(Partitioners[0]) + 1, "No partition implementation for given dimensions");

	return Partitioners[D-1](K, boxMins, boxMaxs, boxCount, outIndices, outPartitions, workMem);
}

template<unsigned D, unsigned K>
bool PartitionBoxes(float *(&boxMins)[D], float *(&boxMaxs)[D], unsigned boxCount, unsigned *outIndices, unsigned (&outPartitions)[K], void *workMem)
{
	return PartitionBoxes<D>(&boxMins[0], &boxMaxs[0], boxCount, outIndices, outPartitions, workMem);
}

template<unsigned D, unsigned K>
bool PartitionBoxes(float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions)
{
	typedef bool(*PartFunc)(unsigned k, float * const *boxMins, float * const *boxMaxs, unsigned boxCount, unsigned *outIndices, unsigned *outPartitions);
	static const PartFunc Partitioners[] = { &PartitionLines, &PartitionBoxes2D, &PartitionBoxes3D };

	static_assert(D >= 1 && D <= sizeof(Partitioners) / sizeof(Partitioners[0]) + 1, "No partition implementation for given dimensions");

	return Partitioners[D-1](K, boxMins, boxMaxs, boxCount, outIndices, outPartitions);
}

template<unsigned D, unsigned K>
bool PartitionBoxes(float *(&boxMins)[D], float *(&boxMaxs)[D], unsigned boxCount, unsigned *outIndices, unsigned(&outPartitions)[K])
{
	return PartitionBoxes<D,K>(&boxMins[0], &boxMaxs[0], boxCount, outIndices, &outPartitions[0]);
}
