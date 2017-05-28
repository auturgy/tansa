#include "blob_detector.h"

using namespace std;


namespace tansa {

BlobDetector::BlobDetector(unsigned width, unsigned height, unsigned nthreads) : forest(width*height) {
	mask.width = width;
	mask.height = height;
	mask.data = (PixelType *) malloc(width * height);
	this->clear_mask();
}

BlobDetector::~BlobDetector() {
	free(mask.data);
}



void BlobDetector::detect(Image *img, vector<ImageBlob> *blobs) {
	threshold_n_mask(img);
	connected_components(img, blobs);
	postprocess_components(img, blobs);
	//component_filter(img, blobs);
}


void BlobDetector::auto_mask(Image *next) {
	threshold_n_mask(next);

	for(unsigned i = 0; i < next->width * next->height; i++) {
		if(next->data[i] != 0) {
			mask.data[i] = 0;
		}
	}
}


void BlobDetector::clear_mask() {
	memset(mask.data, 0xff, mask.width * mask.height);
}

/**
 * Inplace thresholding of an image and masking such a pixel is zeroed if:
 * - it's value is less than or equal to the threshold
 * - or the corresponding entry in the map is zero
 */
void BlobDetector::threshold_n_mask(Image *img) {
	unsigned i;
	unsigned N = img->height * img->width;
	PixelType thresh = this->threshold;

	PixelType *p = img->data;
	PixelType *mp = mask.data;

	for(i = 0; i < N; ++i) {
		if(*p <= thresh || *mp == 0) {
			*p = 0;
		}
		p++;
		mp++;
	}
}


// Should support 16bit number of components
// We should reject components with only one pixel
// Input: a binary image (8bit)
// Output: a labeling (16bit)
// NOTE: This will modify the image inplace
void BlobDetector::connected_components(Image *img, vector<ImageBlob> *blobs) {

	unsigned N = img->width * img->height;
	blobs->resize(0);

	// Previous neighbors in 8-connectivity
	// These are ordered to go from lowest to highest index
	unsigned negativeOffsets[] = {
		img->width + 1, // up-left
		img->width, // up
		img->width - 1, // up-right
		1 // left
	};

	// First pass
	// TODO: This can be parallelized easily by splitting up the image vertically and then doing a combine along all the edges once
	for(unsigned i = 0; i < N; i++) {
		uint8_t *p = img->data + i;

		// Ignore background
		if(*p == 0)
			continue;


		bool hasNeighbors = false;
		unsigned minLabel = 0;
		for(unsigned j = 0; j < 4; j++) {
			uint8_t *pj = p - negativeOffsets[j];

			// Skip invalid or background neighbors
			if(!(negativeOffsets[j] <= i && *pj != 0))
				continue;

			unsigned nIdx = i - negativeOffsets[j];

			unsigned nLbl = forest.findSet(nIdx);

			// TODO: Because the DisjointSets keeps tracks of mins, we don't need to do that here
			if(!hasNeighbors) { // This is the first neighbor
				minLabel = nLbl;
				hasNeighbors = true;
			}
			else { // Merge the other
				if(nLbl < minLabel) {
					forest.unionSets(nLbl, minLabel);
					minLabel = nLbl;
				}
				else {
					forest.unionSets(minLabel, nIdx);
				}
			}

		}

		forest.makeSet(i);

		if(hasNeighbors) { // Merge with min neighbor
			forest.unionSets(minLabel, i);
		}

	}


	// Second pass
	// Assigning ordered labels to each number
	// This converts the image into a labeling from 1-M where M is the number of components
	LabelType nextLabel = 1;
	LabelType *labels = img->data;
	for(unsigned i = 0; i < N; i++) {
		if(img->data[i] == 0) // Skip background pixels
			continue;

		unsigned s = forest.findSetMin(i);

		if(s == i) { // We've encountered the first element of the set

			// More labels than allowed
			if(nextLabel == 0xff)
				continue;

			labels[i] = nextLabel++;

			// TODO: Keep blobs at an allocated size of 255 at all times
			blobs->push_back(ImageBlob());
			(*blobs)[labels[i] - 1].indices.push_back(i);
		}
		else { // Otherwise, choose the label of the root
			labels[i] = labels[s];

			(*blobs)[labels[i] - 1].indices.push_back(i);
		}
	}

}

void BlobDetector::postprocess_components(Image *img, vector<ImageBlob> *blobs) {


	for(unsigned i = 0; i < blobs->size(); i++) {
		ImageBlob &b = (*blobs)[i];

		b.area = b.indices.size();

		// First pass find centroid
		for(unsigned j = 0; j < b.indices.size(); j++) {
			unsigned idx = b.indices[j];
			unsigned x = idx % img->width,
					 y = idx / img->width;

			b.cx += x;
			b.cy += y;
		}

		b.cx /= b.area;
		b.cy /= b.area;


		// Second pass find the radius
		for(unsigned j = 0; j < b.indices.size(); j++) {
			unsigned idx = b.indices[j];
			unsigned x = idx % img->width,
					 y = idx / img->width;

			int dx = x - b.cx,
			 	dy = y - b.cy;

			unsigned r = (dx*dx) + (dy*dy);

			if(r > b.radius) {
				b.radius = r;
			}
		}

		b.radius = sqrt(b.radius);

	}

}


// TODO: Circularity filter, area filter, filter circles that are very close to each other, filter circles enclosed in other circles?

void component_filter(Image *img, vector<ImageBlob> *blobs) {

	for(unsigned i = 0; i < blobs->size(); i++) {
		//if(blobs[i])


	}


}


}
