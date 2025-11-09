package models

type Solution struct {
	FieldWidth  float64 `json:"field_width"`  // The calculated width of the field in arcminutes
	FieldHeight float64 `json:"field_height"` // The calculated height of the field in arcminutes
	Ra          float64 `json:"ra"`           // The Right Ascension of the center of the field in degrees
	Dec         float64 `json:"dec"`          // The Declination of the center of the field in degrees
	Orientation float64 `json:"orientation"`  // The orientation angle of the image from North in degrees
	Pixscale    float64 `json:"pixscale"`     // The pixel scale of the image in arcseconds per pixel
	RaError     float64 `json:"ra_error"`     // The error between the search_ra position and the solution ra position in arcseconds
	DecError    float64 `json:"dec_error"`    // The error between the search_dec position and the solution dec position in arcseconds
}
