using System.Collections;
using System.Collections.Generic;

namespace luna {
    namespace ui {
        public static class draw {
            
            internal const float normal = 255.0f;

            public static Color generate_color ( float r, float g, float b ) 
            {
                 return new Color(r/normal, g/normal, b/normal);
            }
        }   
    }
}    
