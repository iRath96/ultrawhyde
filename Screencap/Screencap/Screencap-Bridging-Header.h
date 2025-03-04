#import <Foundation/Foundation.h>

id createVirtualDisplay(int width, int height, int ppi, int refreshRate, BOOL hiDPI, NSString *name);
unsigned int getDisplayId(id display);
