const float SQRT2 = 1.414214;

// Depth helpers
float linearizeDepth(in float depth, in mat4 proj) {
	depth = depth * 2.0 - 1.0;
	return proj[3][2] / (proj[2][2] + depth);
}

// Hash helpers
vec3 hash(in vec3 value) {
	value  = fract(value * 1234.567);
	value += dot(value, value.yxz + 123.4567);
	return vec3(fract((value.xyz + value.yzx) * value.zxy));
}

// Matrix helpers
vec3 normalizedMul(in mat4 matrix, in vec3 pos) {
	vec4 clip = matrix * vec4(pos, 1.0);
	return clip.xyz / clip.w;
}

// Screen -> view
vec3 screenToView(in vec2 screenPos, in float depth, in mat4 proj) {
	return normalizedMul(
	    inverse(proj), vec3(screenPos, depth) * 2.0 - 1.0
	);
}

vec3 screenToView(in vec2 screenPos, in sampler2D depthTex, in mat4 proj) {
	float depth = texture(depthTex, screenPos).x;
	return screenToView(screenPos, depth, proj);
}

// Raymarch
struct RayMarchResult {
	bool hasHit;
	vec2 coord;
};

RayMarchResult rayMarch(
    in sampler2D depthTex,
    in mat4      proj,
    in float     time,
    in vec3      viewOrigin,
    in vec3      viewDir,
    in float     rayLength,
    in float     bias,
    in int       stepCount,
    in int       refineStepCount
) {
	vec3 start = viewOrigin + viewDir * 0.1;
	vec3 end   = start + viewDir * 0.01;

	vec4 projStart = proj * vec4(start, 1.0);
	vec4 projEnd   = proj * vec4(end, 1.0);

	float invWStart = 1.0 / projStart.w;
	float invWEnd   = 1.0 / projEnd.w;

	vec2 screenStart = projStart.xy * invWStart * 0.5 + 0.5;
	vec2 screenEnd   = projEnd.xy * invWEnd * 0.5 + 0.5;

	{
		vec2 screenDir = normalize(screenEnd - screenStart);
		screenEnd      = screenStart + screenDir * SQRT2;
		if (screenEnd.x < 0.0 || screenEnd.x > 1.0) {
			float xDelta = screenEnd.x < 0.0 ? -screenEnd.x : screenEnd.x - 1.0;
			float xScale = xDelta / abs(screenDir.x);
			screenEnd -= screenDir * xScale;
		}
		if (screenEnd.y < 0.0 || screenEnd.y > 1.0) {
			float yDelta = screenEnd.y < 0.0 ? -screenEnd.y : screenEnd.y - 1.0;
			float yScale = yDelta / abs(screenDir.y);
			screenEnd -= screenDir * yScale;
		}

		end = screenToView(screenEnd, 0.5, proj);

		vec3 projDir = normalize(end);
		float rayLen = (projDir.z * (0.0 - viewOrigin.y) -
		                projDir.y * (0.0 - viewOrigin.z)) /
		               (projDir.z * viewDir.y - projDir.y * viewDir.z);

		const float maxRayLen = 10000.0;
		rayLen                = clamp(rayLen, -maxRayLen, maxRayLen);
		float badRay          = max(step(rayLen, 0.0), step(maxRayLen, rayLen));
		rayLen                = mix(rayLen, maxRayLen, badRay);

		end       = viewOrigin + viewDir * rayLen;
		projEnd   = proj * vec4(end, 1.0);
		invWEnd   = 1.0 / projEnd.w;
		screenEnd = projEnd.xy * invWEnd * 0.5 + 0.5;
	}
	{
		end = normalizedMul(
		    inverse(proj), vec3(screenEnd * 2.0 - 1.0, 0.5)
		);

		vec3  projDir             = normalize(end);
		vec3  commonPlaneNormal   = normalize(cross(viewDir, projDir));
		float projectFromTheRight = step(0.5, abs(commonPlaneNormal.x));
		float rayLenFromRight = (projDir.z * (0.0 - viewOrigin.y) -
		                         projDir.y * (0.0 - viewOrigin.z)) /
		                        (projDir.z * viewDir.y - projDir.y * viewDir.z);
		float rayLenFromTop = (projDir.z * (0.0 - viewOrigin.x) -
		                       projDir.x * (0.0 - viewOrigin.z)) /
		                      (projDir.z * viewDir.x - projDir.x * viewDir.z);
		float rayLen = mix(rayLenFromTop, rayLenFromRight, projectFromTheRight);

		end       = viewOrigin + viewDir * rayLen;
		projEnd   = proj * vec4(end, 1.0);
		invWEnd   = 1.0 / projEnd.w;
		screenEnd = projEnd.xy * invWEnd * 0.5 + 0.5;
	}

	vec3 homoStart = start * invWStart;
	vec3 homoEnd   = end * invWEnd;

	for (int i = 1; i <= stepCount; i++) {
		float rand =
		    hash(float(i) * viewOrigin * viewDir * time).x * 0.0;
		float progress = (float(i) - rand) / float(stepCount);

		progress = 1.0 - progress;
		float angleFactor =
		    pow(max(dot(viewDir, vec3(0.0, 0.0, -1.0)), 0.0), 2.0);
		progress = pow(progress, 1.0 + angleFactor);
		progress = 1.0 - progress;

		float invW   = mix(invWStart, invWEnd, progress);
		float homoZ  = mix(homoStart.z, homoEnd.z, progress);
		vec2  screen = mix(screenStart, screenEnd, progress);

		float rayDepth = -(homoZ / invW);

		float sampleDepth = linearizeDepth(texture(depthTex, screen).x, proj);

		if (rayDepth > sampleDepth) {
			float depthDiff  = rayDepth - sampleDepth;
			vec2  finalCoord = screen;

			float progressDelta = 1.0 / float(stepCount);
			bool  inFront       = false;

			vec2  prevScreen;
			float prevRayDepth;
			for (int j = 0; j < refineStepCount; j++) {
				progressDelta *= 0.5;
				progress      += inFront ? progressDelta : -progressDelta;

				prevScreen   = screen;
				prevRayDepth = rayDepth;

				invW   = mix(invWStart, invWEnd, progress);
				homoZ  = mix(homoStart.z, homoEnd.z, progress);
				screen = mix(screenStart, screenEnd, progress);

				rayDepth = -(homoZ / invW);
				sampleDepth = linearizeDepth(texture(depthTex, screen).x, proj);

				inFront = sampleDepth > rayDepth;

				depthDiff  = inFront ? depthDiff : rayDepth - sampleDepth;
				finalCoord = inFront ? finalCoord : screen;
			}

			float thickness  = rayDepth * 0.08 + 1.0;
			thickness        = pow(thickness, 1.0);
			thickness       -= 1.0;
			if (depthDiff > thickness) {
				break;
			}

			return RayMarchResult(true, finalCoord);
		}
	}

	return RayMarchResult(false, vec2(0.0));
}
