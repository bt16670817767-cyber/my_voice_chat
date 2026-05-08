using UnityEngine;
using UnityEngine.UI;

public class VolumeBarAnimator : MonoBehaviour
{
    private Slider slider;
    private float targetValue;
    private float currentValue;

    void Start()
    {
        slider = GetComponent<Slider>();
        targetValue = Random.Range(0.2f, 0.8f);
    }

    void Update()
    {
        // 平滑过渡到目标值
        currentValue = Mathf.Lerp(currentValue, targetValue, Time.deltaTime * 8f);
        slider.value = currentValue;

        // 每隔一段时间换一个新的目标值，模拟说话起伏
        if (Mathf.Abs(currentValue - targetValue) < 0.05f)
        {
            targetValue = Random.Range(0.1f, 0.9f);
        }
    }
}