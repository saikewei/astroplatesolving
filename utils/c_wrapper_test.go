package utils

import (
	"os"
	"path/filepath"
	"testing"
)

func TestSolve(t *testing.T) {
	// 1. 准备测试数据
	// 读取测试图像文件。路径是相对于项目根目录的。
	imagePath, err := filepath.Abs("/mnt/d/temp/SNY03438.jpg")
	if err != nil {
		t.Fatalf("Failed to get absolute path for test image: %v", err)
	}

	imageData, err := os.ReadFile(imagePath)
	if err != nil {
		t.Fatalf("Failed to read test image '%s': %v", imagePath, err)
	}

	// astrometry.net 索引文件的路径
	// 请确保这个路径是正确的
	indexPath, err := filepath.Abs("/home/saikewei/star_index")
	if err != nil {
		t.Fatalf("Failed to get absolute path for index files: %v", err)
	}

	// 相机焦距
	focalLength := 200.0

	// 2. 调用被测试的函数
	t.Log("Calling C function 'solve_from_memory' via Go wrapper...")
	solution, err := Solve(imageData, indexPath, focalLength)

	// 3. 检查结果
	if err != nil {
		// 如果 err 不为 nil，说明调用失败了。
		// 这可能是因为 .so 文件没找到、链接出错或 C 函数返回了错误码。
		t.Fatalf("Solve() function failed: %v", err)
	}

	if solution == nil {
		t.Fatal("Solve() returned a nil solution without an error.")
	}

	// 如果代码能执行到这里，说明 C 函数被成功调用并且返回了 0 (成功)
	t.Log("Successfully called C function and received a solution!")
	t.Logf("Solution Details: RA=%.6f, Dec=%.6f, Pixscale=%.4f", solution.RA, solution.Dec, solution.Pixscale)

	// 可选：更详细的断言
	// 如果你知道这张测试图片正确的赤经(RA)，可以进行断言
	// if solution.RA < 0.0 || solution.RA > 360.0 {
	// 	t.Errorf("Expected RA to be between 0 and 360, but got %f", solution.RA)
	// }
}
