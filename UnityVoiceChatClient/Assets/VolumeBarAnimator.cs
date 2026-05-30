using UnityEngine;
using UnityEngine.UI;
using TMPro;

public class VolumeBarAnimator : MonoBehaviour
{
    private Slider slider;
    private float targetValue;
    private float currentValue;
    
    public bool isMuted = false;
    
    // 拖按钮里的文字进来
    public TextMeshProUGUI muteButtonText;

    void Start()
    {
        slider = GetComponent<Slider>();
        targetValue = Random.Range(0.2f, 0.8f);
        UpdateMuteButtonText();
    }

    void Update()
    {
        if (isMuted)
        {
            currentValue = Mathf.Lerp(currentValue, 0f, Time.deltaTime * 5f);
            slider.value = currentValue;
            return;
        }

        currentValue = Mathf.Lerp(currentValue, targetValue, Time.deltaTime * 8f);
        slider.value = currentValue;

        if (Mathf.Abs(currentValue - targetValue) < 0.05f)
        {
            targetValue = Random.Range(0.1f, 0.9f);
        }
    }

    public void ToggleMute()
    {
        isMuted = !isMuted;
        UpdateMuteButtonText();
        Debug.Log("Muted: " + isMuted);
    }
    
    void UpdateMuteButtonText()
    {
        if (muteButtonText != null)
        {
            muteButtonText.text = isMuted ? "UNMUTE" : "MUTE";
        }
    }
}