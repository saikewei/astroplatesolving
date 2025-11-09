package utils

/*
   #cgo CFLAGS: -I../clib -I../clib/libs/utils
   #cgo LDFLAGS: -L../clib/build -lastroapi -lstdc++
   #include "astroapi.h"
   #include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

// Go struct mirroring the C Solution struct
type Solution struct {
	FieldWidth  float64
	FieldHeight float64
	RA          float64
	Dec         float64
	Orientation float64
	Pixscale    float64
	RAError     float64
	DecError    float64
}

// Solve 从内存中的图像数据进行天体解析
// imageData: 图像文件的完整字节内容
// indexPath: astrometry.net 索引文件的路径
// focalLength: 焦距（毫米）
func Solve(imageData []byte, indexPath string, focalLength float64) (*Solution, error) {
	// --- 调用 C API ---
	// 准备参数
	cIndexPath := C.CString(indexPath)
	defer C.free(unsafe.Pointer(cIndexPath))

	// 检查 imageData 是否为空
	if len(imageData) == 0 {
		return nil, fmt.Errorf("imageData cannot be empty")
	}

	// Go []byte -> C unsigned char*
	cImageData := (*C.uchar)(unsafe.Pointer(&imageData[0]))
	cImageSize := C.size_t(len(imageData))

	var cSolution C.struct_SolutionForGo

	// 调用 C 函数
	ret := C.solve_from_memory(
		cImageData,
		cImageSize,
		cIndexPath,
		C.double(focalLength),
		&cSolution,
	)

	if ret != 0 {
		return nil, fmt.Errorf("astrometry solving failed with C code: %d", ret)
	}

	// 将 C 结构体转换为 Go 结构体
	solution := &Solution{
		FieldWidth:  float64(cSolution.fieldWidth),
		FieldHeight: float64(cSolution.fieldHeight),
		RA:          float64(cSolution.ra),
		Dec:         float64(cSolution.dec),
		Orientation: float64(cSolution.orientation),
		Pixscale:    float64(cSolution.pixscale),
		RAError:     float64(cSolution.raError),
		DecError:    float64(cSolution.decError),
	}

	return solution, nil
}
