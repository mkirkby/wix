//-------------------------------------------------------------------------------------------------
// <copyright file="wixstrsafe.h" company="Outercurve Foundation">
//   Copyright (c) 2004, Outercurve Foundation.
//   This software is released under Microsoft Reciprocal License (MS-RL).
//   The license and further copyright text can be found in the file LICENSE.TXT
//   LICENSE.TXT at the root directory of the distribution.
// </copyright>
// 
// <summary>
// strsafe.h in the Windows SDK v6.0a that ships in Visual Studio 2008 isn't PREfast-clean. This
// header wraps strsafe.h and adds suppressions for all PREfast warnings. 
// </summary>
//-------------------------------------------------------------------------------------------------

#pragma once
#pragma warning(push)
#pragma warning(disable: 6001 6011 6029 6031 6053 6054 6057 6059 6063 6064 6066 6067 6200 6201 6202 6203 6204 6209 6211 6214 6215 6216 6217 6219 6220 6221 6225 6226 6230 6235 6236 6237 6239 6240 6242 6244 6246 6248 6250 6255 6258 6259 6260 6262 6263 6268 6269 6270 6271 6272 6273 6274 6276 6277 6278 6279 6280 6281 6282 6283 6284 6285 6286 6287 6288 6289 6290 6291 6292 6293 6294 6295 6296 6297 6298 6299 6302 6303 6305 6306 6308 6309 6310 6312 6313 6314 6315 6316 6317 6318 6319 6320 6322 6323 6324 6326 6327 6328 6329 6331 6332 6333 6334 6335 6336 6381 6383 6384 6385 6386 6387 6388 6400 6401 6500 6501 6503 6504 6505 6506 6507 6508 6509 6510 6511 6512 6513 6514 6515 6516 6517 6518 6521 6522 6523 6525 6526 6530 6535 6540 6990 6991 6992 6993 6994 6995)
#include <strsafe.h>
#pragma warning(pop)
