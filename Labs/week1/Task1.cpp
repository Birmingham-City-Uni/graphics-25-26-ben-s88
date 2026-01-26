#include <iostream>
#include <cmath>
#include <lodepng.h>

const int width = 1920, height = 1080;
const int nChannels = 4;

void setPixel(std::vector<uint8_t> *imgBuffer, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	int pixelIdx = x + y * width;
	(*imgBuffer)[pixelIdx * nChannels + 0] = r; // Set red pixel values to 0
	(*imgBuffer)[pixelIdx * nChannels + 1] = g; // Set green pixel values to 255 (full brightness)
	(*imgBuffer)[pixelIdx * nChannels + 2] = b; // Set blue pixel values to 255 (full brightness)
	(*imgBuffer)[pixelIdx * nChannels + 3] = a;
}

int main()
{
	std::string outputFilename = "output.png";

	

	// Setting up an image buffer
	// This std::vector has one 8-bit value for each pixel in each row and column of the image, and
	// for each of the 4 channels (red, green, blue and alpha).
	// Remember 8-bit unsigned values can range from 0 to 255.
	std::vector<uint8_t> imageBuffer(height*width*nChannels);

	// This for loop sets all the pixels of the image to a cyan colour. 

	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x) {
			if (y < height/2)
			{
				setPixel(&imageBuffer, x, y, 0, 255, 0, 255);
			}
			else
			{
				setPixel(&imageBuffer, x, y, 0, 255, 255, 255);
			}
		}


	for(int y = 0; y < height; ++y) 
		for (int x = 0; x < width; ++x) {
			if (sqrt(pow(x - width / 2, 2) + pow(y - height / 2, 2)) < 300)
			{
				setPixel(&imageBuffer, x, y, 255, rand() & 255, rand() & 255, 255);
			}
			else
			{
				setPixel(&imageBuffer, x, y, rand() & 255, rand() & 255, rand() & 255, 255);
			}
		}

	std::cout << imageBuffer.size();
	/// *** Lab Tasks ***
	// * Task 1: Try adapting the code above to set the lower half of the image to be a green colour.
	// * Task 2: Doing the maths above to work out indices is a bit annoying! Write your own setPixel function.
	//           This should take x and y coordinates as input, and red, green, blue and alpha values.
	//           Remember to pass in your imageBuffer. Should it be passed in by reference or by value? Should
	//           the reference be const?
	//           We will use this setPixel function to build our rasteriser in the upcoming labs.
	//			 Test your setPixel function by setting pixels in your image to different colours.
	// * Optional Task 3: Use your setPixel function to draw a circle in the centre of the image. Remember a point is
	//           in a circle if sqrt((x - x_0)^2 + (y - y_0)^2) < radius (here x_0, y_0 are the coordinates at the middle of 
	//           the circle). 
	//           Hint - use a similar for loop to the one above, and add an if statement to check if the current
	//           pixel lies in the circle.
	//           Try modifying the order you draw each component in. If you draw the circle before setting the lower 
	//           part of the image to be green, how does this modify the image?
	// * Optional Task 4: Work out how good the compression ratio of the saved PNG image is. PNG images
	//           use *lossless* compression, where all the pixel values of the original image are preserved.
	//           To work out the compression ratio, compare the size of the saved image to the memory
	//           occupied by the image buffer (this is based on the width, height and number of channels).
	//           Try setting the pixels to random values (use rand() and the % operator). What is the 
	//           compression ratio now, and why do you think this is?
	/*
	* The image buffer vector has a length of 8.2 million 8 bit values when the circle is drawn and the file is 2.7kb
	* 
	* when using the rand funcion for each of the rgba values the file size is instead 5.9mb because the lossless compression algorithm is less effective when
	* there is less solid groups of colours together
	*/


	// *** Encoding image data ***
	// PNG files are compressed to save storage space. 
	// The lodepng::encode function applies this compression to the image buffer and saves the result 
	// to the filename given.
	int errorCode;
	errorCode = lodepng::encode(outputFilename, imageBuffer, width, height);
	if (errorCode) { // check the error code, in case an error occurred.
		std::cout << "lodepng error encoding image: " << lodepng_error_text(errorCode) << std::endl;
		return errorCode;
	}

	return 0;
}
